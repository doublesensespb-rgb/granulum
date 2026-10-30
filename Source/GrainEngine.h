#pragma once
// Granulum — granular engine (Granulizer-style chopping, but deeper).
// Pure C++17, no JUCE dependency.

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
    double sampleRate = 44100.0;

    int numFrames() const   { return channels.empty() ? 0 : (int) channels[0].size(); }
    int numChannels() const { return (int) channels.size(); }
};

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

    const int minGap = std::max (1, (int) (0.06 * s.sampleRate / hop)); // 60 ms
    int last = -minGap;
    float slow = env[0];
    for (int f = 1; f < frames; ++f)
    {
        const float e = env[(size_t) f];
        const float prev = env[(size_t) f - 1];
        const bool rising = e > prev * 1.6f && e > slow * 1.9f;
        const bool loudEnough = e > peak * 0.08f;
        if (rising && loudEnough && f - last >= minGap)
        {
            // walk back to the start of the rise
            int g = f;
            while (g > 0 && env[(size_t) g - 1] < env[(size_t) g] && f - g < 4) --g;
            out.push_back (g * hop);
            last = f;
        }
        slow = slow * 0.9f + e * 0.1f;
    }
    if (out.empty() || out.front() > hop * 4) out.insert (out.begin(), 0);
    return out;
}

struct Params
{
    // grains (the four "Granulizer" knobs)
    float attack      = 0.35f;  // 0 = hard square edges (gritty), 1 = fully smooth
    float holdMs      = 80.0f;  // grain length
    float spacingMs   = 60.0f;  // time between grain starts
    float waveSpacing = 1.0f;   // source advance per grain relative to spacing (-3..3)

    // motion
    float position    = 0.0f;   // start point 0..1
    float rand        = 0.0f;   // scramble grain order 0..1
    float smear       = 0.0f;   // smudge grains into a wash 0..1
    float pan         = 0.5f;   // odd/even grain stereo split 0..1
    float lfoDepth    = 0.0f;   // modulates wave spacing
    float lfoRate     = 0.5f;   // Hz

    // pitch
    float pitch       = 0.0f;   // semitones
    float pitchSpray  = 0.0f;   // +/- semitones random
    float octaveProb  = 0.0f;   // chance a grain jumps an octave
    float reverseProb = 0.0f;   // chance a grain plays backwards

    bool  transients  = false;
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
    bool   hit = false; // transient grain
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
        vizCount.store (0); vizHeadCount.store (0);
    }

    void noteOn (int note, float velocity)
    {
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
        auto& v = *target;
        const bool wasActive = v.active;
        v.active = true; v.note = note; v.velocity = velocity;
        v.stage = Stage::Attack; v.startStamp = ++stamp;
        v.travel = 0.0; v.nextGrainIn = 0.0; v.grainCount = 0; v.lastTransient = -1;
        if (! wasActive) { v.env = 0.0f; for (auto& g : v.grains) g.active = false; }
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.active && v.note == note && v.stage != Stage::Release) v.stage = Stage::Release;
    }

    void allNotesOff() { for (auto& v : voices) if (v.active) v.stage = Stage::Release; }

    void render (float* outL, float* outR, int numSamples, const Params& p, const SampleData* sample)
    {
        if (sample == nullptr || sample->numFrames() < 4)
        {
            for (auto& v : voices) { v.active = false; v.stage = Stage::Off; }
            vizCount.store (0); vizHeadCount.store (0);
            return;
        }

        const int    N        = sample->numFrames();
        const double fileRate = sample->sampleRate / sr;
        const float* srcL     = sample->channels[0].data();
        const float* srcR     = sample->numChannels() > 1 ? sample->channels[1].data() : srcL;

        const float attackInc  = 1.0f / (0.002f * (float) sr);
        const float releaseDec = 1.0f / (0.18f * (float) sr);

        const double spacing  = std::max (16.0, p.spacingMs * 0.001 * sr);
        const double hold     = std::max (16.0, p.holdMs * 0.001 * sr * (1.0 + 7.0 * (double) (p.smear * p.smear)));
        const float  overlap  = (float) std::min ((double) kGrainsPerVoice, hold / spacing);
        const float  norm     = 1.0f / std::sqrt (std::max (1.0f, overlap));
        const double lfoInc   = 2.0 * kPi * (double) p.lfoRate / sr;

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

                v.nextGrainIn -= 1.0;
                if (v.nextGrainIn <= 0.0)
                {
                    spawnGrain (v, p, *sample, fileRate, hold, spacing);

                    // advance through the file: waveSpacing x spacing (in source time)
                    const double lfo = std::sin (lfoPhase + v.lfoOffset);
                    const double ws  = (double) p.waveSpacing + (double) p.lfoDepth * 3.0 * lfo;
                    v.travel += ws * spacing * fileRate / (double) N;
                    v.travel -= std::floor (v.travel);

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

                const float amp = v.env * v.velocity * norm * 1.41421356f;
                outL[i] += sumL * amp;
                outR[i] += sumR * amp;
            }
        }

        lfoPhase = std::fmod (lfoPhase + lfoInc * numSamples, 2.0 * kPi);
        publishViz (N);
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
        double   travel = 0.0;
        double   nextGrainIn = 0.0;
        double   lfoOffset = 0.0;
        uint64_t startStamp = 0;
        uint32_t grainCount = 0;
        int      lastTransient = -1;
        std::array<Grain, kGrainsPerVoice> grains {};
    };

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

        float semis = (float) (v.note - 60) + p.pitch + p.pitchSpray * rng.bipolar() + p.smear * 0.25f * rng.bipolar();
        if (p.octaveProb > 0.0f && rng.uni() < p.octaveProb)
            semis += (rng.uni() < 0.66f) ? 12.0f : -12.0f;
        const double ratio = std::pow (2.0, (double) semis / 12.0) * fileRate;

        // where in the file
        double start = (double) p.position + v.travel;
        if (p.rand > 0.0f)
            start += (double) (p.rand * p.rand) * 0.5 * (double) rng.bipolar();
        start = wrap (start, 1.0) * (double) N;
        if (p.smear > 0.0f) // smudge: scatter grains around the read point
            start += (double) p.smear * (double) rng.bipolar() * (spacing * 6.0 + hold) * fileRate;

        int   length = std::max (16, (int) (hold * (1.0 + 0.3 * (double) p.smear * (double) rng.bipolar())));
        float attack = std::max (p.attack, p.smear);
        bool  hit = false;

        // transients: a grain that crosses an onset starts exactly on it and keeps its punch
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
        if (hit) g->fade = std::max (8, (int) (0.0008 * sr)); // tiny de-click only
        g->inc = reverse ? -ratio : ratio;
        g->pos = reverse ? start + ratio * (double) length : start;
        g->hit = hit;

        // Granulizer-style odd/even stereo split, with a little scatter
        const float side = (v.grainCount++ & 1u) ? 1.0f : -1.0f;
        const float pan = std::clamp (p.pan * side * (0.75f + 0.25f * rng.uni()), -1.0f, 1.0f);
        const float a = (pan + 1.0f) * 0.25f * (float) kPi;
        g->gainL = std::cos (a);
        g->gainR = std::sin (a);
    }

    void publishViz (int N)
    {
        int n = 0, h = 0;
        for (auto& v : voices)
        {
            if (! v.active) continue;
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
    Random rng;
    uint64_t stamp = 0;
};

} // namespace granulum
