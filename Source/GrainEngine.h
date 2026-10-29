#pragma once
// Granulum — granular engine. Pure C++17, no JUCE dependency, so it can be
// unit-tested and rendered offline.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace granulum
{

static constexpr double kPi = 3.14159265358979323846;

struct SampleData
{
    std::vector<std::vector<float>> channels; // 1 or 2 channels
    double sampleRate = 44100.0;

    int numFrames() const   { return channels.empty() ? 0 : (int) channels[0].size(); }
    int numChannels() const { return (int) channels.size(); }
};

struct Params
{
    float position    = 0.3f;   // 0..1 of the file
    float spray       = 0.05f;  // 0..1 random position offset
    float scan        = 0.0f;   // playhead speed, 1 = original speed, 0 = frozen
    float sizeMs      = 120.0f; // grain length
    float density     = 25.0f;  // grains per second per voice
    float chaos       = 0.2f;   // timing + size jitter 0..1
    float pitch       = 0.0f;   // semitones
    float pitchSpray  = 0.0f;   // +/- semitones random
    float octaveProb  = 0.0f;   // chance a grain jumps an octave
    float reverseProb = 0.0f;   // chance a grain plays backwards
    float shape       = 0.0f;   // 0 = smooth (Hann), 0.5 = flat, 1 = percussive
    float spread      = 0.6f;   // stereo scatter 0..1
    float attackMs    = 10.0f;
    float releaseMs   = 400.0f;
};

class Random
{
public:
    explicit Random (uint64_t seed = 0x9E3779B97F4A7C15ull) : s (seed ? seed : 1) {}
    inline uint64_t next()
    {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        return s;
    }
    inline float uni()     { return (float) ((next() >> 40) * (1.0 / 16777216.0)); } // [0,1)
    inline float bipolar() { return uni() * 2.0f - 1.0f; }
private:
    uint64_t s;
};

struct Grain
{
    bool   active = false;
    double pos = 0.0;    // in source frames
    double inc = 1.0;    // source frames per output sample (negative = reverse)
    int    length = 1;
    int    age = 0;
    float  gainL = 0.7f, gainR = 0.7f;
};

class GrainEngine
{
public:
    static constexpr int kMaxVoices     = 8;
    static constexpr int kGrainsPerVoice = 64;
    static constexpr int kMaxViz        = 192;

    // ---- visualisation, written by audio thread, read by UI ----
    std::array<std::atomic<float>, kMaxViz> vizPos {};
    std::array<std::atomic<float>, kMaxViz> vizAmp {};
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
            v.active = false;
            v.stage = Stage::Off;
            v.env = 0.0f;
            for (auto& g : v.grains) g.active = false;
        }
        vizCount.store (0);
        vizHeadCount.store (0);
    }

    void noteOn (int note, float velocity)
    {
        Voice* target = nullptr;
        for (auto& v : voices)
            if (! v.active) { target = &v; break; }

        if (target == nullptr) // steal: prefer released voices, then oldest
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
        v.active = true;
        v.note = note;
        v.velocity = velocity;
        v.stage = Stage::Attack;
        v.startStamp = ++stamp;
        v.scanPos = 0.0;
        v.nextGrainIn = 0.0;
        if (! wasActive)
        {
            v.env = 0.0f;
            for (auto& g : v.grains) g.active = false;
        }
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.active && v.note == note && v.stage != Stage::Release)
                v.stage = Stage::Release;
    }

    void allNotesOff()
    {
        for (auto& v : voices)
            if (v.active) v.stage = Stage::Release;
    }

    // Adds into outL/outR (caller clears them).
    void render (float* outL, float* outR, int numSamples, const Params& p, const SampleData* sample)
    {
        if (sample == nullptr || sample->numFrames() < 4)
        {
            // nothing to play; still let envelopes die
            for (auto& v : voices) if (v.stage == Stage::Release) { v.active = false; v.stage = Stage::Off; }
            vizCount.store (0);
            vizHeadCount.store (0);
            return;
        }

        const int    N        = sample->numFrames();
        const double fileRate = sample->sampleRate / sr;
        const float* srcL     = sample->channels[0].data();
        const float* srcR     = sample->numChannels() > 1 ? sample->channels[1].data() : srcL;

        const float attackInc = 1.0f / std::max (1.0f, p.attackMs * 0.001f * (float) sr);
        const float releaseDec = 1.0f / std::max (1.0f, p.releaseMs * 0.001f * (float) sr);

        const double baseInterval = sr / std::max (0.1f, p.density);
        const double grainLen     = std::max (8.0, p.sizeMs * 0.001 * sr);
        const float  overlap      = std::min ((float) kGrainsPerVoice, (float) (p.density * p.sizeMs * 0.001f));
        const float  norm         = 1.0f / std::sqrt (std::max (1.0f, overlap));
        const double scanInc      = (double) p.scan * fileRate / (double) N;

        for (auto& v : voices)
        {
            if (! v.active) continue;

            for (int i = 0; i < numSamples; ++i)
            {
                // ---- envelope ----
                switch (v.stage)
                {
                    case Stage::Attack:
                        v.env += attackInc;
                        if (v.env >= 1.0f) { v.env = 1.0f; v.stage = Stage::Sustain; }
                        break;
                    case Stage::Release:
                        v.env -= releaseDec;
                        if (v.env <= 0.0f) { v.env = 0.0f; v.stage = Stage::Off; }
                        break;
                    default: break;
                }

                if (v.stage == Stage::Off)
                {
                    v.active = false;
                    for (auto& g : v.grains) g.active = false;
                    break;
                }

                // ---- playhead scan ----
                v.scanPos += scanInc;
                if (v.scanPos >= 1.0) v.scanPos -= std::floor (v.scanPos);
                if (v.scanPos < 0.0)  v.scanPos -= std::floor (v.scanPos);

                // ---- scheduler ----
                v.nextGrainIn -= 1.0;
                if (v.nextGrainIn <= 0.0)
                {
                    spawnGrain (v, p, N, fileRate, grainLen);
                    const double jitter = 1.0 + (double) p.chaos * (double) rng.bipolar() * 0.95;
                    v.nextGrainIn += std::max (1.0, baseInterval * jitter);
                }

                // ---- render grains ----
                float sumL = 0.0f, sumR = 0.0f;
                for (auto& g : v.grains)
                {
                    if (! g.active) continue;

                    const float t = (float) g.age / (float) g.length;
                    const float w = window (t, p.shape);

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

        publishViz (N);
    }

    int activeVoiceCount() const
    {
        int n = 0;
        for (auto& v : voices) n += v.active ? 1 : 0;
        return n;
    }

    static float window (float t, float shape)
    {
        t = std::min (1.0f, std::max (0.0f, t));
        const float hann = 0.5f - 0.5f * std::cos (2.0f * (float) kPi * t);
        float edge = std::min (t, 1.0f - t) / 0.12f;
        edge = std::min (1.0f, edge);
        const float trap = edge * edge * (3.0f - 2.0f * edge); // smoothstep fades
        if (shape <= 0.5f)
            return hann + (trap - hann) * (shape * 2.0f);

        float perc;
        if (t < 0.015f) perc = t / 0.015f;
        else
        {
            const float d = 1.0f - (t - 0.015f) / 0.985f;
            perc = d * d * d;
        }
        return trap + (perc - trap) * ((shape - 0.5f) * 2.0f);
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
        double   scanPos = 0.0;
        double   nextGrainIn = 0.0;
        uint64_t startStamp = 0;
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
        const int   i1 = (int) pos;
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

    void spawnGrain (Voice& v, const Params& p, int N, double fileRate, double grainLen)
    {
        Grain* g = nullptr;
        for (auto& c : v.grains)
            if (! c.active) { g = &c; break; }
        if (g == nullptr) return;

        // pitch
        float semis = (float) (v.note - 60) + p.pitch + p.pitchSpray * rng.bipolar();
        if (p.octaveProb > 0.0f && rng.uni() < p.octaveProb)
            semis += (rng.uni() < 0.66f) ? 12.0f : -12.0f; // shimmer leans upward
        const double ratio = std::pow (2.0, (double) semis / 12.0) * fileRate;

        // position
        double centre = (double) p.position + v.scanPos + (double) (p.spray * rng.bipolar());
        centre = wrap (centre, 1.0);

        // length with chaos
        const double len = grainLen * (1.0 + 0.5 * (double) p.chaos * (double) rng.bipolar());

        const bool reverse = p.reverseProb > 0.0f && rng.uni() < p.reverseProb;

        g->active = true;
        g->age = 0;
        g->length = std::max (8, (int) len);
        g->inc = reverse ? -ratio : ratio;
        // centre the grain around the chosen position
        g->pos = centre * (double) N - g->inc * (double) g->length * 0.5;

        const float pan = p.spread * rng.bipolar();                // -1..1
        const float a = (pan + 1.0f) * 0.25f * (float) kPi;        // 0..pi/2
        g->gainL = std::cos (a);
        g->gainR = std::sin (a);
    }

    void publishViz (int N)
    {
        int n = 0;
        int h = 0;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            if (h < kMaxVoices)
                vizHead[(size_t) h++].store ((float) v.scanPos, std::memory_order_relaxed);
            for (auto& g : v.grains)
            {
                if (! g.active || n >= kMaxViz) continue;
                vizPos[(size_t) n].store ((float) (wrap (g.pos, (double) N) / (double) N), std::memory_order_relaxed);
                vizAmp[(size_t) n].store (window ((float) g.age / (float) g.length, 0.0f) * v.env, std::memory_order_relaxed);
                ++n;
            }
        }
        vizCount.store (n, std::memory_order_release);
        vizHeadCount.store (h, std::memory_order_release);
    }

    double sr = 44100.0;
    std::array<Voice, kMaxVoices> voices {};
    Random rng;
    uint64_t stamp = 0;
};

} // namespace granulum
