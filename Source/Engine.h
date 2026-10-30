#pragma once
// Granulum v2 — bass granulizer engine. Pure C++17, no JUCE dependency.
//
// Per voice:  SUB (sine, mono, straight out)
//             ADD (16 harmonics)  -> pre-grain ring buffer -> pulsar grains
//             SAMPLER (mono WAV, 100 Hz high-pass) -> pre-grain ring buffer -> pulsar grains
//
// Grains fire once per period of the played note, so the pitch always stays on the note.
// TONE = grain playback speed (formant), TEXTURE = grain size + window hardness,
// MOTION = how far back in the buffer grains read (+ stereo width), AMOUNT = dry/grain mix.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace granulum
{

static constexpr double kPi = 3.14159265358979323846;
static constexpr int kNumHarmonics = 16;

// ---------------------------------------------------------------------------
struct SineTable
{
    static constexpr int N = 4096;
    float t[N + 1];
    SineTable() { for (int i = 0; i <= N; ++i) t[i] = (float) std::sin (2.0 * kPi * i / N); }
    inline float operator() (double cycles) const
    {
        cycles -= std::floor (cycles);
        const double p = cycles * N;
        const int i = (int) p;
        const float f = (float) (p - i);
        return t[i] + (t[i + 1] - t[i]) * f;
    }
};
inline const SineTable& sineTable() { static const SineTable s; return s; }

// ---------------------------------------------------------------------------
struct SampleData
{
    std::vector<float> data;          // mono
    double sampleRate = 44100.0;
    double detectedRoot = 60.0;       // MIDI note (fractional)
    bool pitched = false;
    int numFrames() const { return (int) data.size(); }
};

// YIN pitch detection on the loudest part of the sample. Returns Hz or 0.
inline double detectPitchHz (const std::vector<float>& x, double sr)
{
    int n = 8192;
    if ((int) x.size() < 2048) return 0.0;
    if ((int) x.size() < n) n = (int) x.size();

    int best = 0;
    double bestE = -1.0;
    for (int s = 0; s + n <= (int) x.size(); s += 2048)
    {
        double e = 0.0;
        for (int i = s; i < s + n; i += 4) e += (double) x[(size_t) i] * x[(size_t) i];
        if (e > bestE) { bestE = e; best = s; }
    }
    const float* w = x.data() + best;
    const int tauMin = std::max (2, (int) (sr / 1500.0));
    const int tauMax = std::min (n / 2, (int) (sr / 27.0));
    const int W = n - tauMax;
    if (W < 256) return 0.0;

    std::vector<double> d ((size_t) tauMax + 2, 0.0), c ((size_t) tauMax + 2, 1.0);
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        double s = 0.0;
        for (int i = 0; i < W; ++i) { const double v = (double) w[i] - (double) w[i + tau]; s += v * v; }
        d[(size_t) tau] = s;
    }
    double run = 0.0;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        run += d[(size_t) tau];
        c[(size_t) tau] = run > 0.0 ? d[(size_t) tau] * tau / run : 1.0;
    }

    int tau = -1;
    for (int t = tauMin; t <= tauMax; ++t)
        if (c[(size_t) t] < 0.15)
        {
            while (t + 1 <= tauMax && c[(size_t) t + 1] < c[(size_t) t]) ++t;
            tau = t;
            break;
        }
    if (tau < 0)
    {
        double m = 1e9; int mt = -1;
        for (int t = tauMin; t <= tauMax; ++t) if (c[(size_t) t] < m) { m = c[(size_t) t]; mt = t; }
        if (m < 0.35) tau = mt; else return 0.0;
    }
    double bt = tau;
    if (tau > 1 && tau < tauMax)
    {
        const double a = c[(size_t) tau - 1], b = c[(size_t) tau], cc = c[(size_t) tau + 1];
        const double den = a - 2.0 * b + cc;
        if (std::abs (den) > 1e-12) bt = tau + 0.5 * (a - cc) / den;
    }
    return sr / bt;
}

// ---------------------------------------------------------------------------
struct LayerParams
{
    float pitch = 0.0f;    // semitones (oct * 12 + semi, continuous under modulation)
    bool  run   = true;    // false = skip (volume 0)
    bool  grain = true;    // false = plain oscillator / sampler
};

struct Params
{
    LayerParams sub, add, smp;
    float tone = 0.0f;       // semitones, -12..12
    float texture = 0.25f;   // 0..1
    float motion = 0.0f;     // 0..1
    float amount = 1.0f;     // 0..1
    float globalPitch = 0.0f;
    bool  mono = true;
    float glideMs = 40.0f;
    float smpRoot = 60.0f;   // MIDI note at which the sample plays unshifted
    std::array<float, kNumHarmonics> harmonics {};
};

class Engine
{
public:
    static constexpr int kMaxVoices = 8;
    static constexpr int kMaxGrains = 10;

    // visual feedback
    std::atomic<uint32_t> grainEvents[2] { { 0 }, { 0 } };   // ADD, SAMPLER
    std::atomic<uint32_t> noteEvents { 0 };
    std::atomic<float> vizFreq { 55.0f };
    std::atomic<int> vizVoices { 0 };

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0 ? sampleRate : 44100.0;
        lookahead = (int) std::ceil (0.19 * sr) + 32;
        const int need = lookahead + (int) (0.3 * sr) + 1024;
        ringSize = 1;
        while (ringSize < need) ringSize <<= 1;
        mask = ringSize - 1;
        for (auto& v : voices)
        {
            v.add.buf.assign ((size_t) ringSize, 0.0f);
            v.smp.buf.assign ((size_t) ringSize, 0.0f);
        }
        smoothCoef = 1.0 - std::exp (-1.0 / (0.008 * sr));

        // 4th-order Butterworth high-pass at 100 Hz for the sampler (two biquads)
        makeHighPass (100.0, 0.54119610, hpA);
        makeHighPass (100.0, 1.30656296, hpB);
        reset();
    }

    void reset()
    {
        for (auto& v : voices) { v.active = false; v.env = 0.0f; }
        stackSize = 0; heldCount = 0; lastPitch = -1.0;
        vizVoices.store (0);
    }

    void noteOn (int note, float velocity, const Params& p, const SampleData* smp)
    {
        const bool overlap = heldCount > 0;
        ++heldCount;
        noteEvents.fetch_add (1, std::memory_order_relaxed);
        const bool glide = p.glideMs > 0.5f && lastPitch >= 0.0 && overlap;

        if (p.mono)
        {
            pushNote (note);
            auto& v = voices[0];
            for (size_t i = 1; i < voices.size(); ++i)
                if (voices[i].active) voices[i].stage = Stage::Release;
            if (v.active && v.stage != Stage::Release)
            {
                v.note = note;
                v.vel = velocity;
                glideTo (v, note, p);
                lastPitch = note;
                return;
            }
            startVoice (v, note, velocity, p, smp, glide ? lastPitch : (double) note);
            if (glide) startGlide (v, lastPitch, note, p);
            lastPitch = note;
            return;
        }

        Voice* target = nullptr;
        for (auto& v : voices) if (! v.active) { target = &v; break; }
        if (target == nullptr)
        {
            int64_t oldest = INT64_MAX;
            for (auto& v : voices)
            {
                const int64_t score = (int64_t) v.stamp - (v.stage == Stage::Release ? (int64_t (1) << 40) : 0);
                if (score < oldest) { oldest = score; target = &v; }
            }
        }
        startVoice (*target, note, velocity, p, smp, glide ? lastPitch : (double) note);
        if (glide) startGlide (*target, lastPitch, note, p);
        lastPitch = note;
    }

    void noteOff (int note, const Params& p)
    {
        heldCount = std::max (0, heldCount - 1);
        if (p.mono)
        {
            removeNote (note);
            auto& v = voices[0];
            if (! v.active) return;
            if (stackSize > 0)
            {
                const int back = noteStack[(size_t) stackSize - 1];
                v.note = back;
                glideTo (v, back, p);
                lastPitch = back;
            }
            else v.stage = Stage::Release;
            return;
        }
        for (auto& v : voices)
            if (v.active && v.note == note && v.stage != Stage::Release) v.stage = Stage::Release;
    }

    void allNotesOff()
    {
        stackSize = 0; heldCount = 0;
        for (auto& v : voices) if (v.active) v.stage = Stage::Release;
    }

    // Adds into the output buffers (caller clears them).
    void render (float* subOut, float* addL, float* addR, float* smpL, float* smpR,
                 int n, const Params& p, const SampleData* smp)
    {
        const float attackInc  = 1.0f / (0.003f * (float) sr);
        const float releaseDec = 1.0f / (0.15f * (float) sr);
        const bool hasSmp = smp != nullptr && smp->numFrames() > 4;
        const auto& sine = sineTable();

        int activeCount = 0;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            ++activeCount;

            for (int i = 0; i < n; ++i)
            {
                if (v.stage == Stage::Attack)
                {
                    v.env += attackInc;
                    if (v.env >= 1.0f) { v.env = 1.0f; v.stage = Stage::Sustain; }
                }
                else if (v.stage == Stage::Release)
                {
                    v.env -= releaseDec;
                    if (v.env <= 0.0f) { v.env = 0.0f; v.active = false; break; }
                }

                if (v.glideAge < v.glideLen)
                {
                    ++v.glideAge;
                    v.cur = v.glideFrom + (v.glideTarget - v.glideFrom) * (double) v.glideAge / (double) v.glideLen;
                }

                const float amp = v.env * (0.35f + 0.65f * v.vel);
                const double base = v.cur + (double) p.globalPitch;

                // ---- SUB ----
                v.subPitch += (base + (double) p.sub.pitch - v.subPitch) * smoothCoef;
                if (p.sub.run)
                {
                    const double f = 440.0 * std::exp2 ((v.subPitch - 69.0) / 12.0);
                    v.subPhase += f / sr;
                    if (v.subPhase >= 1.0) v.subPhase -= std::floor (v.subPhase);
                    subOut[i] += sine (v.subPhase) * amp * 0.8f;
                }

                // ---- ADD ----
                {
                    auto& L = v.add;
                    L.pitch += (base + (double) p.add.pitch - L.pitch) * smoothCoef;
                    if (p.add.run)
                    {
                        const double f = 440.0 * std::exp2 ((L.pitch - 69.0) / 12.0);
                        L.buf[(size_t) (L.w & mask)] = addSource (L, f, p);
                        ++L.w;
                        float oL, oR;
                        processLayer (L, f, p, p.add.grain, oL, oR, 0);
                        addL[i] += oL * amp;
                        addR[i] += oR * amp;
                    }
                }

                // ---- SAMPLER ----
                if (hasSmp)
                {
                    auto& L = v.smp;
                    L.pitch += (base + (double) p.smp.pitch - L.pitch) * smoothCoef;
                    if (p.smp.run)
                    {
                        const double f = 440.0 * std::exp2 ((L.pitch - 69.0) / 12.0);
                        L.buf[(size_t) (L.w & mask)] = smpSource (L, L.pitch, *smp, p.smpRoot);
                        ++L.w;
                        float oL, oR;
                        processLayer (L, f, p, p.smp.grain, oL, oR, 1);
                        smpL[i] += oL * amp;
                        smpR[i] += oR * amp;
                    }
                }
            }
            if (v.active)
                vizFreq.store ((float) (440.0 * std::exp2 ((v.add.pitch - 69.0) / 12.0)), std::memory_order_relaxed);
        }
        vizVoices.store (activeCount, std::memory_order_relaxed);
    }

    int activeVoiceCount() const
    {
        int c = 0;
        for (auto& v : voices) c += v.active ? 1 : 0;
        return c;
    }

private:
    enum class Stage { Off, Attack, Sustain, Release };

    struct Grain
    {
        bool on = false;
        double pos = 0.0, inc = 1.0;
        int len = 1, age = 0, fade = 1;
        float gl = 1.0f, gr = 1.0f, norm = 1.0f;
    };

    struct Layer
    {
        std::vector<float> buf;
        int64_t w = 0, start = 0;     // write counter, write index when the note began
        double phase = 0.0;           // ADD: cycles, SAMPLER: frames
        double pitch = 60.0;          // smoothed MIDI pitch
        double nextGrain = 0.0;
        uint32_t k = 0, seed = 1;
        std::array<Grain, kMaxGrains> g {};
        float h1[4] {}, h2[4] {};     // sampler high-pass states
    };

    struct Voice
    {
        bool active = false;
        int note = 60;
        float vel = 1.0f;
        Stage stage = Stage::Off;
        float env = 0.0f;
        double cur = 60.0, glideFrom = 60.0, glideTarget = 60.0;
        int glideLen = 0, glideAge = 0;
        double subPhase = 0.0, subPitch = 60.0;
        uint64_t stamp = 0;
        Layer add, smp;
    };

    static inline float hash01 (uint32_t x)
    {
        x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
        return (float) (x >> 8) / 16777216.0f;
    }

    void makeHighPass (double fc, double q, float* c)
    {
        const double w0 = 2.0 * kPi * fc / sr, cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        c[0] = (float) (((1.0 + cw) / 2.0) / a0);
        c[1] = (float) (-(1.0 + cw) / a0);
        c[2] = c[0];
        c[3] = (float) ((-2.0 * cw) / a0);
        c[4] = (float) ((1.0 - alpha) / a0);
    }

    static inline float biquad (const float* c, float* s, float x)
    {
        const float y = c[0] * x + c[1] * s[0] + c[2] * s[1] - c[3] * s[2] - c[4] * s[3];
        s[1] = s[0]; s[0] = x; s[3] = s[2]; s[2] = y;
        return y;
    }

    float addSource (Layer& L, double freq, const Params& p)
    {
        const auto& sine = sineTable();
        const double lim = 0.45 * sr;
        float v = 0.0f;
        for (int k = 1; k <= kNumHarmonics; ++k)
        {
            if ((double) k * freq > lim) break;
            const float a = p.harmonics[(size_t) k - 1];
            if (a > 0.0005f) v += a * sine (L.phase * (double) k);
        }
        L.phase += freq / sr;
        if (L.phase >= 1.0) L.phase -= std::floor (L.phase);
        return v * 0.5f;
    }

    float smpSource (Layer& L, double pitch, const SampleData& s, float root)
    {
        const int N = s.numFrames();
        const double ratio = s.sampleRate / sr * std::exp2 ((pitch - (double) root) / 12.0);
        const int i0 = (int) L.phase;
        const float f = (float) (L.phase - i0);
        const int i1 = i0 + 1 >= N ? 0 : i0 + 1;
        float v = s.data[(size_t) i0] + (s.data[(size_t) i1] - s.data[(size_t) i0]) * f;
        L.phase += ratio;
        if (L.phase >= (double) N) L.phase = std::fmod (L.phase, (double) N);
        v = biquad (hpA, L.h1, v);
        v = biquad (hpB, L.h2, v);
        return v;
    }

    inline float readBuf (const Layer& L, double pos) const
    {
        const int64_t i = (int64_t) std::floor (pos);
        const float f = (float) (pos - (double) i);
        const float a = L.buf[(size_t) (i & mask)];
        const float b = L.buf[(size_t) ((i + 1) & mask)];
        return a + (b - a) * f;
    }

    void processLayer (Layer& L, double freq, const Params& p, bool grainOn, float& outL, float& outR, int layerIndex)
    {
        const int64_t play = L.w - 1 - lookahead;
        const float dry = play >= L.start ? L.buf[(size_t) (play & mask)] : 0.0f;

        if (! grainOn || p.amount <= 0.0005f)
        {
            outL = outR = dry;
            return;
        }

        // ---- schedule: one grain per period of the note ----
        const double period = sr / freq;
        L.nextGrain -= 1.0;
        if (L.nextGrain <= 0.0)
        {
            spawnGrain (L, period, play, p);
            grainEvents[layerIndex].fetch_add (1, std::memory_order_relaxed);
            L.nextGrain += period;
        }

        float sl = 0.0f, srr = 0.0f;
        for (auto& g : L.g)
        {
            if (! g.on) continue;
            const float a = (float) std::min (g.age + 1, g.len - g.age) / (float) g.fade;
            float w = 1.0f;
            if (a < 1.0f) w = 0.5f - 0.5f * std::cos ((float) kPi * std::max (0.0f, a));
            const float s = readBuf (L, g.pos) * w * g.norm;
            sl += s * g.gl;
            srr += s * g.gr;
            g.pos += g.inc;
            if (++g.age >= g.len) g.on = false;
        }

        const float wet = p.amount;
        outL = dry * (1.0f - wet) + sl * wet;
        outR = dry * (1.0f - wet) + srr * wet;
    }

    void spawnGrain (Layer& L, double period, int64_t play, const Params& p)
    {
        Grain* g = nullptr;
        for (auto& c : L.g) if (! c.on) { g = &c; break; }
        if (g == nullptr) return;

        const double t = std::clamp ((double) p.texture, 0.0, 1.0);
        const double sizeCycles = 4.0 * std::pow (16.0, -t);               // 4 -> 0.25 periods
        const double rate = std::exp2 ((double) p.tone / 12.0);             // formant speed
        // PSOLA-style: a grain covers sizeCycles periods of the source, resampled by the formant rate,
        // and grains are spaced one period apart -> pitch stays on the note, the spectral envelope moves.
        const double span = std::min (sizeCycles * period, (double) (lookahead - 16));
        const int len = (int) std::max (8.0, span / rate);

        const double hard = std::clamp ((t - 0.15) / 0.7, 0.0, 1.0);
        const double fadeFrac = 0.5 * (1.0 - hard) + 0.04 * hard;

        const float u1 = hash01 (L.seed + L.k * 2654435761u);
        const float u2 = hash01 (L.seed ^ (L.k * 2246822519u + 1u));
        const double m = std::clamp ((double) p.motion, 0.0, 1.0);
        double start = (double) play - std::pow (m, 1.5) * 0.25 * sr * u1;
        start = std::max (start, (double) L.start);
        start = std::max (start, (double) (L.w - ringSize + 16));

        g->on = true;
        g->age = 0;
        g->len = len;
        g->fade = std::max (4, (int) (fadeFrac * len));
        g->inc = rate;
        g->pos = start;
        // keep loudness steady: overlapping grains add coherently, sparse pulses lose energy
        const double cover = (len / period) * (1.0 - fadeFrac);
        g->norm = (float) std::min (3.0, cover >= 1.0 ? 1.0 / cover : 1.0 / std::sqrt (std::max (0.02, cover)));

        const float width = (float) m * 0.8f;
        const float pan = width * ((L.k & 1u) ? 1.0f : -1.0f) * (0.4f + 0.6f * u2);
        const float ang = (pan + 1.0f) * 0.25f * (float) kPi;
        g->gl = std::cos (ang) * 1.41421356f;
        g->gr = std::sin (ang) * 1.41421356f;
        ++L.k;
    }

    void prerollLayer (Layer& L, Voice& v, const Params& p, const SampleData* smp, bool isAdd, double startNote)
    {
        L.start = L.w;
        L.nextGrain = 0.0;
        L.k = 0;
        L.seed = (uint32_t) (v.note * 7919 + 17);
        for (auto& g : L.g) g.on = false;
        L.pitch = startNote + (double) p.globalPitch + (double) (isAdd ? p.add.pitch : p.smp.pitch);
        const double f = 440.0 * std::exp2 ((L.pitch - 69.0) / 12.0);
        if (isAdd)
        {
            L.phase = 0.0;
            for (int i = 0; i < lookahead; ++i) { L.buf[(size_t) (L.w & mask)] = addSource (L, f, p); ++L.w; }
        }
        else if (smp != nullptr && smp->numFrames() > 4)
        {
            L.phase = 0.0;
            for (auto& s : L.h1) s = 0.0f;
            for (auto& s : L.h2) s = 0.0f;
            for (int i = 0; i < lookahead; ++i) { L.buf[(size_t) (L.w & mask)] = smpSource (L, L.pitch, *smp, p.smpRoot); ++L.w; }
        }
        else
        {
            for (int i = 0; i < lookahead; ++i) { L.buf[(size_t) (L.w & mask)] = 0.0f; ++L.w; }
        }
    }

    void startVoice (Voice& v, int note, float velocity, const Params& p, const SampleData* smp, double startNote)
    {
        const bool fresh = ! v.active;
        v.active = true; v.note = note; v.vel = velocity;
        v.cur = v.glideFrom = v.glideTarget = startNote; v.glideLen = v.glideAge = 0;
        v.stage = Stage::Attack; v.stamp = ++stamp;
        if (fresh) { v.env = 0.0f; v.subPhase = 0.0; }
        v.subPitch = startNote + (double) p.globalPitch + (double) p.sub.pitch;
        prerollLayer (v.add, v, p, smp, true, startNote);
        prerollLayer (v.smp, v, p, smp, false, startNote);
    }

    void startGlide (Voice& v, double from, double to, const Params& p)
    {
        v.glideFrom = from;
        v.glideTarget = to;
        v.glideLen = std::max (1, (int) (p.glideMs * 0.001 * sr));
        v.glideAge = 0;
        v.cur = from;
    }

    void glideTo (Voice& v, double to, const Params& p)
    {
        if (p.glideMs > 0.5f) startGlide (v, v.cur, to, p);
        else { v.cur = v.glideFrom = v.glideTarget = to; v.glideLen = v.glideAge = 0; }
    }

    void pushNote (int note)
    {
        removeNote (note);
        if (stackSize < (int) noteStack.size()) noteStack[(size_t) stackSize++] = note;
    }

    void removeNote (int note)
    {
        int w = 0;
        for (int i = 0; i < stackSize; ++i)
            if (noteStack[(size_t) i] != note) noteStack[(size_t) w++] = noteStack[(size_t) i];
        stackSize = w;
    }

    double sr = 44100.0, smoothCoef = 0.01;
    int lookahead = 8400, ringSize = 32768;
    int64_t mask = 32767;
    float hpA[5] {}, hpB[5] {};
    std::array<Voice, kMaxVoices> voices {};
    std::array<int, 32> noteStack {};
    int stackSize = 0, heldCount = 0;
    double lastPitch = -1.0;
    uint64_t stamp = 0;
};

} // namespace granulum
