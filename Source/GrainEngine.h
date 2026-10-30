#pragma once
// Granulum — bass granulizer engine. Pure C++17, no JUCE dependency.

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
    double sampleRate = 44100.0;              // rate at which the data plays at "original pitch" on MIDI note 60

    int numFrames() const   { return channels.empty() ? 0 : (int) channels[0].size(); }
    int numChannels() const { return (int) channels.size(); }
};

// ---------------------------------------------------------------------------
// Additive source: 32 harmonics -> seamless looping buffer with slowly
// "breathing" partials, so wave spacing scrubs through a living timbre.
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

    // deterministic per-harmonic drift (integer number of cycles over the buffer => seamless loop)
    uint32_t seed = 0x1234567u;
    auto rnd = [&seed]() { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 16777216.0f; };

    auto& out = s.channels[0];
    for (int k = 1; k <= kNumHarmonics; ++k)
    {
        const float a = amps[(size_t) k - 1];
        if (a <= 0.0005f || k * c4 > 20000.0) continue;
        const int    driftCycles = 1 + (int) (rnd() * 3.0f);
        const double driftPhase  = rnd() * 2.0 * kPi;
        const float  driftDepth  = 0.25f + 0.5f * rnd() * std::min (1.0f, (float) k / 4.0f); // upper partials move more
        const int    phaseOff    = (int) (rnd() * period);
        for (int i = 0; i < N; ++i)
        {
            const float drift = 1.0f - driftDepth * 0.5f * (1.0f + (float) std::sin (2.0 * kPi * driftCycles * (double) i / N + driftPhase));
            out[(size_t) i] += a * drift * sine[(size_t) ((k * i + phaseOff) % period)];
        }
    }

    float peak = 0.0f;
    for (float v : out) peak = std::max (peak, std::abs (v));
    if (peak > 1.0e-6f)
        for (auto& v : out) v *= 0.89f / peak;
    return s;
}

// Simple energy-flux onset detector.
inline std::vector<int> detectTransients (const SampleData& s)
{
    std::vector<int> out;
    const int N = s.numFrames();
    if (N < 1024) return out;

    const int hop = std::max (64, (int) (s.sampleRate * 0.005)); // 5 ms
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
    float attack      = 0.35f;
    float holdMs      = 80.0f;
    float spacingMs   = 60.0f;
    float waveSpacing = 1.0f;

    float position    = 0.0f;
    float rand        = 0.0f;
    float smear       = 0.0f;
    float pan         = 0.5f;
    float lfoDepth    = 0.0f;
    float lfoRate     = 0.5f;

    float pitch       = 0.0f;   // semitones
    float pitchSpray  = 0.0f;
    float octaveProb  = 0.0f;
    float reverseProb = 0.0f;

    bool  transients  = false;
    bool  keySync     = false;  // grain rate = note frequency -> tonal bass from any source
    bool  mono        = false;
    float glideMs     = 0.0f;
    float sub         = 0.0f;   // sine sub level
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
    std::array<std::atomic<float>, kMaxVoices> vizHead {};
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
        stackSize = 0;
        vizCount.store (0); vizHeadCount.store (0);
    }

    void noteOn (int note, float velocity, bool mono)
    {
        if (mono)
        {
            pushNote (note);
            auto& v = voices[0];
            for (size_t i = 1; i < voices.size(); ++i)
                if (voices[i].active) voices[i].stage = Stage::Release;

            const bool legato = v.active && v.stage != Stage::Release;
            v.targetNote = note;
            v.velocity = velocity;
            if (legato) return;                     // glide, no retrigger
            startVoice (v, note, velocity, ! v.active);
            return;
        }

        Voice* target = nullptr;
        for (size_t i = 0; i < voices.size(); ++i) if (! voices[i].active) { target = &voices[i]; break; }
        if (target == nullptr)
        {
            int64_t oldest = INT64_MAX;
            for (auto& v : voices)
            {
                const int64_t score = (int64_t) v.startStamp - (v.stage == Stage::Release ? (int64_t (1) << 40) : 0);
                if (score < oldest) { oldest = score; target = &v; }
            }
        }
        startVoice (*target, note, velocity, ! target->active);
    }

    void noteOff (int note, bool mono)
    {
        if (mono)
        {
            removeNote (note);
            auto& v = voices[0];
            if (! v.active) return;
            if (stackSize > 0) v.targetNote = noteStack[(size_t) stackSize - 1]; // fall back legato
            else               v.stage = Stage::Release;
            return;
        }
        for (auto& v : voices)
            if (v.active && v.note == note && v.stage != Stage::Release) v.stage = Stage::Release;
    }

    void allNotesOff()
    {
        stackSize = 0;
        for (auto& v : voices) if (v.active) v.stage = Stage::Release;
    }

    // Grains are added into outL/outR, the clean sine sub into subOut.
    void render (float* outL, float* outR, float* subOut, int numSamples, const Params& p, const SampleData* sample)
    {
        const bool hasSample = sample != nullptr && sample->numFrames() >= 4;
        const int    N        = hasSample ? sample->numFrames() : 1;
        const double fileRate = hasSample ? sample->sampleRate / sr : 1.0;
        const float* srcL     = hasSample ? sample->channels[0].data() : nullptr;
        const float* srcR     = hasSample ? (sample->numChannels() > 1 ? sample->channels[1].data() : srcL) : nullptr;

        const float  attackInc  = 1.0f / (0.002f * (float) sr);
        const float  releaseDec = 1.0f / (0.18f * (float) sr);
        const double holdBase   = std::max (16.0, p.holdMs * 0.001 * sr * (1.0 + 7.0 * (double) (p.smear * p.smear)));
        const double lfoInc     = 2.0 * kPi * (double) p.lfoRate / sr;
        const double glideCoef  = p.glideMs > 0.5f ? std::exp (-1.0 / (p.glideMs * 0.001 * sr)) : 0.0;

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

                // glide
                v.curNote = (double) v.targetNote + (v.curNote - (double) v.targetNote) * glideCoef;
                const double freq = 440.0 * std::pow (2.0, (v.curNote - 69.0) / 12.0);

                // sub
                v.subPhase += 2.0 * kPi * freq / sr;
                if (v.subPhase > 2.0 * kPi) v.subPhase -= 2.0 * kPi;
                const float amp = v.env * v.velocity;
                subOut[i] += (float) std::sin (v.subPhase) * amp * p.sub * 0.8f;

                if (! hasSample) continue;

                v.nextGrainIn -= 1.0;
                if (v.nextGrainIn <= 0.0)
                {
                    const double spacing = p.keySync ? sr / freq : std::max (16.0, p.spacingMs * 0.001 * sr);
                    spawnGrain (v, p, *sample, fileRate, holdBase, spacing);

                    const double lfo = std::sin (lfoPhase + v.lfoOffset);
                    const double ws  = (double) p.waveSpacing + (double) p.lfoDepth * 3.0 * lfo;
                    v.travel += ws * spacing * fileRate / (double) N;
                    v.travel -= std::floor (v.travel);

                    const float overlap = (float) std::min ((double) kGrainsPerVoice, holdBase / spacing);
                    v.norm = 1.0f / std::sqrt (std::max (1.0f, overlap));
                    v.nextGrainIn += spacing;
                }

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

        lfoPhase = std::fmod (lfoPhase + lfoInc * numSamples, 2.0 * kPi);
        publishViz (N, hasSample);
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
        int      targetNote = 60;
        double   curNote = 60.0;
        float    velocity = 1.0f;
        Stage    stage = Stage::Off;
        float    env = 0.0f;
        float    norm = 1.0f;
        double   travel = 0.0;
        double   nextGrainIn = 0.0;
        double   lfoOffset = 0.0;
        double   subPhase = 0.0;
        uint64_t startStamp = 0;
        uint32_t grainCount = 0;
        int      lastTransient = -1;
        std::array<Grain, kGrainsPerVoice> grains {};
    };

    void startVoice (Voice& v, int note, float velocity, bool fresh)
    {
        v.active = true; v.note = note; v.targetNote = note; v.curNote = note;
        v.velocity = velocity; v.stage = Stage::Attack; v.startStamp = ++stamp;
        v.travel = 0.0; v.nextGrainIn = 0.0; v.grainCount = 0; v.lastTransient = -1;
        if (fresh) { v.env = 0.0f; v.subPhase = 0.0; for (auto& g : v.grains) g.active = false; }
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

    void spawnGrain (Voice& v, const Params& p, const SampleData& s, double fileRate, double hold, double spacing)
    {
        Grain* g = nullptr;
        for (auto& c : v.grains) if (! c.active) { g = &c; break; }
        if (g == nullptr) return;

        const int N = s.numFrames();

        // grains always follow the played note; key sync additionally locks the grain rate to it
        float semis = (float) (v.curNote - 60.0) + p.pitch
                    + p.pitchSpray * rng.bipolar() + p.smear * 0.25f * rng.bipolar();
        if (p.octaveProb > 0.0f && rng.uni() < p.octaveProb)
            semis += (rng.uni() < 0.66f) ? 12.0f : -12.0f;
        const double ratio = std::pow (2.0, (double) semis / 12.0) * fileRate;

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

        const bool reverse = ! hit && p.reverseProb > 0.0f && rng.uni() < p.reverseProb;

        g->active = true;
        g->age = 0;
        g->length = length;
        g->fade = std::max (8, (int) (attack * (float) length * 0.5f));
        if (hit) g->fade = std::max (8, (int) (0.0008 * sr));
        g->inc = reverse ? -ratio : ratio;
        g->pos = reverse ? start + ratio * (double) length : start;
        g->hit = hit;

        const float side = (v.grainCount++ & 1u) ? 1.0f : -1.0f;
        const float pan = std::clamp (p.pan * side * (0.75f + 0.25f * rng.uni()), -1.0f, 1.0f);
        const float a = (pan + 1.0f) * 0.25f * (float) kPi;
        g->gainL = std::cos (a);
        g->gainR = std::sin (a);
    }

    void publishViz (int N, bool hasSample)
    {
        int n = 0, h = 0;
        for (auto& v : voices)
        {
            if (! v.active || ! hasSample) continue;
            if (h < kMaxVoices) vizHead[(size_t) h++].store ((float) v.travel, std::memory_order_relaxed);
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
    double lfoPhase = 0.0;
    std::array<Voice, kMaxVoices> voices {};
    std::array<int, 32> noteStack {};
    int stackSize = 0;
    Random rng;
    uint64_t stamp = 0;
};

} // namespace granulum
