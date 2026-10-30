#pragma once
// Granulum — bass granulizer engine (one instance per oscillator). Pure C++17, no JUCE dependency.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace granulum
{

static constexpr double kPi = 3.14159265358979323846;

struct SampleData
{
    std::vector<std::vector<float>> channels; // 1 or 2 channels
    std::vector<int> transients;              // onset positions in frames, sorted
    double sampleRate = 44100.0;              // plays at "original pitch" on MIDI note 60

    int numFrames() const   { return channels.empty() ? 0 : (int) channels[0].size(); }
    int numChannels() const { return (int) channels.size(); }
};

// ---------------------------------------------------------------------------
// Additive oscillator: 32 harmonics -> seamless looping buffer with slowly
// "breathing" partials. Levels are absolute: lower bars = quieter, all zero = silence.
static constexpr int kNumHarmonics = 32;

inline SampleData renderAdditive (const std::array<float, kNumHarmonics>& amps)
{
    constexpr int period = 184;           // samples per cycle
    constexpr int cycles = 1044;          // ~4 s
    constexpr int N = period * cycles;
    constexpr double c4 = 261.6255653;    // plays at true C4 on note 60

    SampleData s;
    s.sampleRate = period * c4;
    s.channels.assign (1, std::vector<float> ((size_t) N, 0.0f));

    std::array<float, period> sine {};
    for (int i = 0; i < period; ++i)
        sine[(size_t) i] = (float) std::sin (2.0 * kPi * i / period);

    uint32_t seed = 0x1234567u;
    auto rnd = [&seed]() { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 16777216.0f; };

    auto& out = s.channels[0];
    for (int k = 1; k <= kNumHarmonics; ++k)
    {
        const float a = amps[(size_t) k - 1];
        // keep the random stream identical regardless of which bars are up
        const int    driftCycles = 1 + (int) (rnd() * 3.0f);
        const double driftPhase  = rnd() * 2.0 * kPi;
        const float  driftDepth  = (k == 1 ? 0.0f : 0.2f + 0.4f * rnd() * std::min (1.0f, (float) k / 4.0f));
        const int    phaseOff    = k == 1 ? 0 : (int) (rnd() * period);
        if (a <= 0.0005f || k * c4 > 20000.0) continue;
        for (int i = 0; i < N; ++i)
        {
            const float drift = 1.0f - driftDepth * 0.5f * (1.0f + (float) std::sin (2.0 * kPi * driftCycles * (double) i / N + driftPhase));
            out[(size_t) i] += a * drift * sine[(size_t) ((k * i + phaseOff) % period)];
        }
    }

    // absolute level: one full bar = 0.7 peak; only scale down if it would clip
    for (auto& v : out) v *= 0.7f;
    float peak = 0.0f;
    for (float v : out) peak = std::max (peak, std::abs (v));
    if (peak > 0.95f)
        for (auto& v : out) v *= 0.95f / peak;
    return s;
}

// Simple energy-flux onset detector.
inline std::vector<int> detectTransients (const SampleData& s)
{
    std::vector<int> out;
    const int N = s.numFrames();
    if (N < 1024) return out;

    const int hop = std::max (64, (int) (s.sampleRate * 0.005));
    const int frames = N / hop;
    std::vector<float> env ((size_t) frames, 0.0f);
    for (int f = 0; f < frames; ++f)
    {
        double e = 0.0;
        for (const auto& ch : s.channels)
            for (int i = f * hop; i < (f + 1) * hop; ++i)
                e += (double) ch[(size_t) i] * ch[(size_t) i];
        env[(size_t) f] = (float) std::sqrt (e / (hop * (double) s.numChannels()));
    }

    float peak = 0.0f;
    for (float e : env) peak = std::max (peak, e);
    if (peak <= 1.0e-6f) return out;

    const int minGap = std::max (1, (int) (0.06 * s.sampleRate / hop));
    int last = -minGap;
    float slow = env[0];
    for (int f = 1; f < frames; ++f)
    {
        const float e = env[(size_t) f];
        const float prev = env[(size_t) f - 1];
        if (e > prev * 1.6f && e > slow * 1.9f && e > peak * 0.08f && f - last >= minGap)
        {
            int g = f;
            while (g > 0 && env[(size_t) g - 1] < env[(size_t) g] && f - g < 4) --g;
            out.push_back (g * hop);
            last = f;
        }
        slow = slow * 0.9f + e * 0.1f;
    }
    if (! out.empty() && out.front() > hop * 4) out.insert (out.begin(), 0);
    return out;
}

// ---------------------------------------------------------------------------

struct Params
{
    // granular
    float attack      = 0.35f;
    float holdMs      = 80.0f;
    float spacingMs   = 60.0f;
    float waveSpacing = 1.0f;
    float position    = 0.0f;
    float rand        = 0.0f;
    float smear       = 0.0f;
    float pan         = 0.5f;
    bool  transients  = false;
    bool  keySync     = false;  // grain rate = note frequency
    bool  grainOn     = true;   // false = plain looping oscillator / sampler

    // oscillator
    float transpose   = 0.0f;   // semitones (oct * 12 + semi)

    // voicing
    bool  mono        = false;
    float portaMs     = 0.0f;
    bool  portaAlways = false;  // glide on every note, not only overlapping ones
    bool  portaScaled = false;  // glide time grows with the interval
};

class Random
{
public:
    explicit Random (uint64_t seed = 0x9E3779B97F4A7C15ull) : s (seed ? seed : 1) {}
    inline uint64_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
    inline float uni()     { return (float) ((next() >> 40) * (1.0 / 16777216.0)); }
    inline float bipolar() { return uni() * 2.0f - 1.0f; }
private:
    uint64_t s;
};

struct Grain
{
    bool   active = false;
    double pos = 0.0;
    double inc = 1.0;
    int    length = 1;
    int    fade = 1;
    int    age = 0;
    float  gainL = 0.7f, gainR = 0.7f;
    bool   hit = false;
};

class GrainEngine
{
public:
    static constexpr int kMaxVoices      = 8;
    static constexpr int kGrainsPerVoice = 64;
    static constexpr int kMaxViz         = 192;

    std::array<std::atomic<float>, kMaxViz> vizPos {};
    std::array<std::atomic<float>, kMaxViz> vizAmp {};
    std::array<std::atomic<int>,   kMaxViz> vizHit {};
    std::atomic<int> vizCount { 0 };
    std::array<std::atomic<float>, kMaxVoices> vizHead {};   // absolute read position 0..1
    std::atomic<int> vizHeadCount { 0 };

    void prepare (double hostSampleRate)
    {
        sr = hostSampleRate > 0 ? hostSampleRate : 44100.0;
        reset();
    }

    void reset()
    {
        for (auto& v : voices)
        {
            v.active = false; v.stage = Stage::Off; v.env = 0.0f;
            for (auto& g : v.grains) g.active = false;
        }
        stackSize = 0; heldCount = 0; lastPitch = -1.0;
        vizCount.store (0); vizHeadCount.store (0);
    }

    void noteOn (int note, float velocity, const Params& p)
    {
        const bool overlap = heldCount > 0;
        ++heldCount;
        const bool porta = p.portaMs > 0.5f && lastPitch >= 0.0 && (p.portaAlways || overlap);

        if (p.mono)
        {
            pushNote (note);
            auto& v = voices[0];
            for (size_t i = 1; i < voices.size(); ++i)
                if (voices[i].active) voices[i].stage = Stage::Release;

            if (v.active && v.stage != Stage::Release)   // legato: no retrigger
            {
                v.note = note;
                v.velocity = velocity;
                glideTo (v, note, p, p.portaMs > 0.5f);
                lastPitch = note;
                return;
            }
            startVoice (v, note, velocity, p, ! v.active);
            if (porta) startGlide (v, lastPitch, note, p);
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
                const int64_t score = (int64_t) v.startStamp - (v.stage == Stage::Release ? (int64_t (1) << 40) : 0);
                if (score < oldest) { oldest = score; target = &v; }
            }
        }
        startVoice (*target, note, velocity, p, ! target->active);
        if (porta) startGlide (*target, lastPitch, note, p);
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
                glideTo (v, back, p, p.portaMs > 0.5f);
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

    // Adds into outL/outR. sample == nullptr keeps voices/glides running silently.
    void render (float* outL, float* outR, int numSamples, const Params& p, const SampleData* sample)
    {
        const bool hasSample = sample != nullptr && sample->numFrames() >= 4;
        const int    N        = hasSample ? sample->numFrames() : 1;
        const double fileRate = hasSample ? sample->sampleRate / sr : 1.0;
        const float* srcL     = hasSample ? sample->channels[0].data() : nullptr;
        const float* srcR     = hasSample ? (sample->numChannels() > 1 ? sample->channels[1].data() : srcL) : nullptr;

        const float  attackInc  = 1.0f / (0.002f * (float) sr);
        const float  releaseDec = 1.0f / (0.12f * (float) sr);
        const double holdBase   = std::max (16.0, p.holdMs * 0.001 * sr * (1.0 + 7.0 * (double) (p.smear * p.smear)));

        for (auto& v : voices)
        {
            if (! v.active) continue;

            for (int i = 0; i < numSamples; ++i)
            {
                if (v.stage == Stage::Attack)
                {
                    v.env += attackInc;
                    if (v.env >= 1.0f) { v.env = 1.0f; v.stage = Stage::Sustain; }
                }
                else if (v.stage == Stage::Release)
                {
                    v.env -= releaseDec;
                    if (v.env <= 0.0f)
                    {
                        v.env = 0.0f; v.stage = Stage::Off; v.active = false;
                        for (auto& g : v.grains) g.active = false;
                        break;
                    }
                }

                // portamento (linear in pitch, fixed time)
                if (v.glideAge < v.glideLen)
                {
                    ++v.glideAge;
                    v.curNote = v.glideFrom + (v.glideTarget - v.glideFrom) * (double) v.glideAge / (double) v.glideLen;
                }

                if (! hasSample) continue;

                const double semis = v.curNote - 60.0 + (double) p.transpose;
                const double ratio = std::exp2 (semis / 12.0) * fileRate;
                const float  amp   = v.env * v.velocity;

                if (! p.grainOn)
                {
                    // plain looping oscillator / sampler
                    float l, r;
                    readHermite (srcL, srcR, N, v.playPos, l, r);
                    v.playPos += ratio;
                    if (v.playPos >= (double) N) v.playPos -= (double) N;
                    outL[i] += l * amp * 0.85f;
                    outR[i] += r * amp * 0.85f;
                }
                else
                {
                    v.nextGrainIn -= 1.0;
                    if (v.nextGrainIn <= 0.0)
                    {
                        const double freq = 440.0 * std::exp2 ((v.curNote + (double) p.transpose - 69.0) / 12.0);
                        const double spacing = p.keySync ? sr / freq : std::max (16.0, p.spacingMs * 0.001 * sr);
                        spawnGrain (v, p, *sample, ratio, holdBase, spacing, fileRate);

                        v.travel += (double) p.waveSpacing * spacing * fileRate / (double) N;
                        v.travel -= std::floor (v.travel);

                        const float overlap = (float) std::min ((double) kGrainsPerVoice, holdBase / spacing);
                        v.norm = 1.0f / std::sqrt (std::max (1.0f, overlap));
                        v.nextGrainIn += spacing;
                    }
                }

                // grains (also lets running grains finish after switching to plain mode)
                float sumL = 0.0f, sumR = 0.0f;
                for (auto& g : v.grains)
                {
                    if (! g.active) continue;
                    const float w = window (g.age, g.length, g.fade);
                    float l, r;
                    readHermite (srcL, srcR, N, g.pos, l, r);
                    sumL += l * w * g.gainL;
                    sumR += r * w * g.gainR;
                    g.pos += g.inc;
                    if (++g.age >= g.length) g.active = false;
                }
                const float ga = amp * v.norm * 1.41421356f;
                outL[i] += sumL * ga;
                outR[i] += sumR * ga;
            }
        }

        publishViz (N, hasSample, p);
    }

    int activeVoiceCount() const
    {
        int n = 0;
        for (auto& v : voices) n += v.active ? 1 : 0;
        return n;
    }

    static inline float window (int age, int length, int fade)
    {
        float a = std::min (1.0f, (float) (age + 1) / (float) fade);
        float b = std::min (1.0f, (float) (length - age) / (float) fade);
        a = a * a * (3.0f - 2.0f * a);
        b = b * b * (3.0f - 2.0f * b);
        return std::min (a, b);
    }

private:
    enum class Stage { Off, Attack, Sustain, Release };

    struct Voice
    {
        bool     active = false;
        int      note = 60;
        float    velocity = 1.0f;
        Stage    stage = Stage::Off;
        float    env = 0.0f;
        float    norm = 1.0f;
        double   curNote = 60.0, glideFrom = 60.0, glideTarget = 60.0;
        int      glideLen = 0, glideAge = 0;
        double   travel = 0.0;
        double   nextGrainIn = 0.0;
        double   playPos = 0.0;
        uint64_t startStamp = 0;
        uint32_t grainCount = 0;
        int      lastTransient = -1;
        std::array<Grain, kGrainsPerVoice> grains {};
    };

    void startVoice (Voice& v, int note, float velocity, const Params& p, bool fresh)
    {
        v.active = true; v.note = note; v.velocity = velocity;
        v.curNote = v.glideFrom = v.glideTarget = note; v.glideLen = v.glideAge = 0;
        v.stage = Stage::Attack; v.startStamp = ++stamp;
        v.travel = 0.0; v.nextGrainIn = 0.0; v.grainCount = 0; v.lastTransient = -1;
        v.playPos = (double) std::clamp (p.position, 0.0f, 0.9999f) * (double) lastNumFrames;
        if (fresh) { v.env = 0.0f; for (auto& g : v.grains) g.active = false; }
    }

    void startGlide (Voice& v, double from, double to, const Params& p)
    {
        double len = p.portaMs * 0.001 * sr;
        if (p.portaScaled) len *= std::max (0.1, std::abs (to - from) / 12.0);
        v.glideFrom = from;
        v.glideTarget = to;
        v.glideLen = std::max (1, (int) len);
        v.glideAge = 0;
        v.curNote = from;
    }

    void glideTo (Voice& v, double to, const Params& p, bool withPorta)
    {
        if (withPorta) startGlide (v, v.curNote, to, p);
        else { v.curNote = v.glideFrom = v.glideTarget = to; v.glideLen = v.glideAge = 0; }
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

    static inline double wrap (double x, double n)
    {
        x = std::fmod (x, n);
        return x < 0.0 ? x + n : x;
    }

    static inline void readHermite (const float* L, const float* R, int N, double pos, float& outL, float& outR)
    {
        pos = wrap (pos, (double) N);
        const int   i1 = std::min (N - 1, (int) pos);
        const float f  = (float) (pos - (double) i1);
        const int i0 = i1 == 0 ? N - 1 : i1 - 1;
        const int i2 = i1 + 1 >= N ? i1 + 1 - N : i1 + 1;
        const int i3 = i2 + 1 >= N ? i2 + 1 - N : i2 + 1;
        auto herm = [f] (float y0, float y1, float y2, float y3)
        {
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        };
        outL = herm (L[i0], L[i1], L[i2], L[i3]);
        outR = (R == L) ? outL : herm (R[i0], R[i1], R[i2], R[i3]);
    }

    void spawnGrain (Voice& v, const Params& p, const SampleData& s, double baseRatio, double hold, double spacing, double fileRate)
    {
        Grain* g = nullptr;
        for (auto& c : v.grains) if (! c.active) { g = &c; break; }
        if (g == nullptr) return;

        const int N = s.numFrames();
        const double ratio = p.smear > 0.0f ? baseRatio * std::exp2 ((double) (p.smear * 0.25f * rng.bipolar()) / 12.0) : baseRatio;

        double start = (double) p.position + v.travel;
        if (p.rand > 0.0f)
            start += (double) (p.rand * p.rand) * 0.5 * (double) rng.bipolar();
        start = wrap (start, 1.0) * (double) N;
        if (p.smear > 0.0f)
            start += (double) p.smear * (double) rng.bipolar() * (spacing * 6.0 + hold) * fileRate;

        int   length = std::max (16, (int) (hold * (1.0 + 0.3 * (double) p.smear * (double) rng.bipolar())));
        float attack = std::max (p.attack, p.smear);
        bool  hit = false;

        if (p.transients && ! s.transients.empty())
        {
            const double span = (double) length * ratio;
            auto it = std::lower_bound (s.transients.begin(), s.transients.end(), (int) start);
            if (it != s.transients.end() && (double) *it < start + span)
            {
                const int idx = (int) (it - s.transients.begin());
                if (idx != v.lastTransient)
                {
                    start = (double) *it;
                    length = std::max (length, (int) (0.09 * sr));
                    attack = 0.0f;
                    hit = true;
                    v.lastTransient = idx;
                }
            }
        }

        g->active = true;
        g->age = 0;
        g->length = length;
        g->fade = hit ? std::max (8, (int) (0.0008 * sr)) : std::max (8, (int) (attack * (float) length * 0.5f));
        g->inc = ratio;
        g->pos = start;
        g->hit = hit;

        const float side = (v.grainCount++ & 1u) ? 1.0f : -1.0f;
        const float pan = std::clamp (p.pan * side * (0.75f + 0.25f * rng.uni()), -1.0f, 1.0f);
        const float a = (pan + 1.0f) * 0.25f * (float) kPi;
        g->gainL = std::cos (a);
        g->gainR = std::sin (a);
    }

    void publishViz (int N, bool hasSample, const Params& p)
    {
        if (hasSample) lastNumFrames = N;
        int n = 0, h = 0;
        for (auto& v : voices)
        {
            if (! v.active || ! hasSample) continue;
            if (h < kMaxVoices)
            {
                const double head = p.grainOn ? wrap ((double) p.position + v.travel, 1.0) : v.playPos / (double) N;
                vizHead[(size_t) h++].store ((float) head, std::memory_order_relaxed);
            }
            for (auto& g : v.grains)
            {
                if (! g.active || n >= kMaxViz) continue;
                vizPos[(size_t) n].store ((float) (wrap (g.pos, (double) N) / (double) N), std::memory_order_relaxed);
                vizAmp[(size_t) n].store (window (g.age, g.length, std::max (1, g.length / 2)) * v.env, std::memory_order_relaxed);
                vizHit[(size_t) n].store (g.hit ? 1 : 0, std::memory_order_relaxed);
                ++n;
            }
        }
        vizCount.store (n, std::memory_order_release);
        vizHeadCount.store (h, std::memory_order_release);
    }

    double sr = 44100.0;
    std::array<Voice, kMaxVoices> voices {};
    std::array<int, 32> noteStack {};
    int stackSize = 0, heldCount = 0, lastNumFrames = 1;
    double lastPitch = -1.0;
    Random rng;
    uint64_t stamp = 0;
};

} // namespace granulum
