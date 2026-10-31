#pragma once
// Granulum — granulizer engine (one instance per oscillator). Pure C++17, no JUCE dependency.
//
// The granular section is driven by macros (BLOOM, SMEAR, SCATTER, SHIMMER, DRIFT, SIZE, SPEED,
// POSITION, DENSITY, MIX, EVOLVE). Each macro moves several internal grain parameters at once.

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
    double sampleRate = 44100.0;              // plays at "original pitch" on MIDI note 60

    int numFrames() const   { return channels.empty() ? 0 : (int) channels[0].size(); }
    int numChannels() const { return (int) channels.size(); }
};

// ---------------------------------------------------------------------------
// Additive oscillator: 16 harmonics -> seamless looping buffer with slowly
// "breathing" partials. Levels are absolute: lower bars = quieter, all zero = silence.
static constexpr int kNumHarmonics = 16;

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
        const int    driftCycles = 1 + (int) (rnd() * 3.0f);
        const double driftPhase  = rnd() * 2.0 * kPi;
        const float  driftDepth  = (k == 1 ? 0.0f : 0.1f + 0.25f * rnd() * std::min (1.0f, (float) k / 4.0f));
        const int    phaseOff    = k == 1 ? 0 : (int) (rnd() * period);
        if (a <= 0.0005f || k * c4 > 20000.0) continue;
        for (int i = 0; i < N; ++i)
        {
            const float drift = 1.0f - driftDepth * 0.5f * (1.0f + (float) std::sin (2.0 * kPi * driftCycles * (double) i / N + driftPhase));
            out[(size_t) i] += a * drift * sine[(size_t) ((k * i + phaseOff) % period)];
        }
    }

    for (auto& v : out) v *= 0.7f;
    float peak = 0.0f;
    for (float v : out) peak = std::max (peak, std::abs (v));
    if (peak > 0.95f)
        for (auto& v : out) v *= 0.95f / peak;
    return s;
}

// ---------------------------------------------------------------------------

struct Params
{
    // granular macros (0..1 unless noted)
    float bloom    = 0.0f;
    float smear    = 0.0f;
    float scatter  = 0.0f;
    float shimmer  = 0.0f;
    float drift    = 0.0f;
    float size     = 0.35f;
    float speed    = 1.0f;   // -2..2, 0 = freeze
    float position = 0.0f;
    float density  = 0.4f;
    float mix      = 1.0f;
    float evolve   = 0.0f;   // seconds, 0 = off
    bool  grainOn  = false;

    // oscillator
    float transpose = 0.0f;  // semitones
    std::array<int, 5> chord { 0, 0, 0, 0, 0 };
    int chordSize = 1;

    // voicing
    bool  mono    = true;
    float portaMs = 0.0f;
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
    float  pan = 0.0f;
    uint8_t kind = 0;   // 0 normal, 1 shimmer, 2 reverse
};

// A grain that was just born — the UI turns these into flying shards.
struct GrainEvent
{
    float pos = 0.0f;    // 0..1 in the source
    float pan = 0.0f;    // -1..1
    float len = 0.0f;    // seconds
    uint8_t kind = 0;
};

class GrainEngine
{
public:
    static constexpr int kMaxVoices      = 16;
    static constexpr int kGrainsPerVoice = 48;
    static constexpr int kMaxViz         = 256;
    static constexpr int kEventRing      = 256;

    // visualisation
    std::array<std::atomic<float>, kMaxViz> vizPos {};
    std::array<std::atomic<float>, kMaxViz> vizAmp {};
    std::atomic<int> vizCount { 0 };
    std::array<std::atomic<float>, kMaxVoices> vizHead {};
    std::atomic<int> vizHeadCount { 0 };
    std::array<GrainEvent, kEventRing> events {};
    std::atomic<uint32_t> eventWrite { 0 };
    std::atomic<float> lastNote { 60.0f };

    void prepare (double hostSampleRate)
    {
        sr = hostSampleRate > 0 ? hostSampleRate : 44100.0;
        driftCoef = 1.0 - std::exp (-1.0 / (0.6 * sr));
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
        lastNote.store ((float) note, std::memory_order_relaxed);
        const int n = std::clamp (p.chordSize, 1, 5);
        const float chordGain = 1.0f / std::sqrt ((float) n);
        const bool porta = p.portaMs > 0.5f && lastPitch >= 0.0 && overlap;

        if (p.mono)
        {
            pushNote (note);
            for (int i = n; i < kMaxVoices; ++i)
                if (voices[(size_t) i].active) voices[(size_t) i].stage = Stage::Release;

            const bool legato = voices[0].active && voices[0].stage != Stage::Release;
            for (int i = 0; i < n; ++i)
            {
                auto& v = voices[(size_t) i];
                const int target = note + p.chord[(size_t) i];
                v.root = note;
                v.chordGain = chordGain;
                if (legato && v.active && v.stage != Stage::Release)
                {
                    v.velocity = velocity;
                    glideTo (v, target, p);
                }
                else
                {
                    startVoice (v, target, velocity, p, ! v.active);
                    v.root = note;
                    v.chordGain = chordGain;
                    if (porta) startGlide (v, lastPitch + p.chord[(size_t) i], target, p);
                }
            }
            lastPitch = note;
            return;
        }

        for (int i = 0; i < n; ++i)
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
            const int pitch = note + p.chord[(size_t) i];
            startVoice (*target, pitch, velocity, p, ! target->active);
            target->root = note;
            target->chordGain = chordGain;
            if (porta) startGlide (*target, lastPitch + p.chord[(size_t) i], pitch, p);
        }
        lastPitch = note;
    }

    void noteOff (int note, const Params& p)
    {
        heldCount = std::max (0, heldCount - 1);
        if (p.mono)
        {
            removeNote (note);
            if (stackSize > 0)
            {
                const int back = noteStack[(size_t) stackSize - 1];
                const int n = std::clamp (p.chordSize, 1, 5);
                for (int i = 0; i < n; ++i)
                {
                    auto& v = voices[(size_t) i];
                    if (! v.active) continue;
                    v.root = back;
                    glideTo (v, back + p.chord[(size_t) i], p);
                }
                lastPitch = back;
                lastNote.store ((float) back, std::memory_order_relaxed);
            }
            else
                for (auto& v : voices) if (v.active) v.stage = Stage::Release;
            return;
        }
        for (auto& v : voices)
            if (v.active && v.root == note && v.stage != Stage::Release) v.stage = Stage::Release;
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

        const float attackInc  = 1.0f / (0.003f * (float) sr);
        const float releaseDec = 1.0f / (0.15f * (float) sr);

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

                if (v.glideAge < v.glideLen)
                {
                    ++v.glideAge;
                    v.curNote = v.glideFrom + (v.glideTarget - v.glideFrom) * (double) v.glideAge / (double) v.glideLen;
                }
                ++v.age;

                // drift walkers: slow, smooth random motion
                for (int w = 0; w < 4; ++w)
                {
                    if (--v.walkCount[w] <= 0)
                    {
                        v.walkTarget[w] = rng.bipolar();
                        v.walkCount[w] = (int) ((0.5 + 2.0 * rng.uni()) * sr);
                    }
                    v.walk[w] += (float) ((v.walkTarget[w] - v.walk[w]) * driftCoef);
                }

                if (! hasSample) continue;

                const double semis = v.curNote - 60.0 + (double) p.transpose;
                const double ratio = std::exp2 (semis / 12.0) * fileRate;
                const float  amp   = v.env * v.velocity * v.chordGain;

                // dry: plain looping playback
                float dl, dr;
                readHermite (srcL, srcR, N, v.playPos, dl, dr);
                v.playPos += ratio;
                if (v.playPos >= (double) N) v.playPos -= (double) N;

                if (! p.grainOn)
                {
                    outL[i] += dl * amp * 0.85f;
                    outR[i] += dr * amp * 0.85f;
                    continue;
                }

                const float ev = p.evolve > 0.01f ? (float) std::min (1.0, (double) v.age / (p.evolve * sr)) : 1.0f;
                const float bloom = p.bloom * ev;

                v.nextGrainIn -= 1.0;
                if (v.nextGrainIn <= 0.0)
                {
                    const double dens = 3.0 * std::pow (60.0, (double) p.density) * (1.0 + 2.5 * bloom);
                    double spacing = sr / dens;
                    spawnGrain (v, p, *sample, ratio, spacing, fileRate, bloom);
                    v.travel += (double) p.speed * spacing * fileRate / (double) N;
                    v.travel -= std::floor (v.travel);
                    spacing *= 1.0 + (double) p.scatter * 0.6 * (double) rng.bipolar();
                    v.nextGrainIn += std::max (8.0, spacing);
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

                const float wet = p.mix * ev * ev;
                const float ga = v.norm * 1.2f;
                outL[i] += (dl * 0.85f * (1.0f - wet) + sumL * ga * wet) * amp;
                outR[i] += (dr * 0.85f * (1.0f - wet) + sumR * ga * wet) * amp;
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
        int      note = 60, root = 60;
        float    velocity = 1.0f, chordGain = 1.0f;
        Stage    stage = Stage::Off;
        float    env = 0.0f;
        float    norm = 1.0f;
        double   curNote = 60.0, glideFrom = 60.0, glideTarget = 60.0;
        int      glideLen = 0, glideAge = 0;
        double   travel = 0.0;
        double   nextGrainIn = 0.0;
        double   playPos = 0.0;
        int64_t  age = 0;
        uint64_t startStamp = 0;
        float    walk[4] {}, walkTarget[4] {};
        int      walkCount[4] {};
        std::array<Grain, kGrainsPerVoice> grains {};
    };

    void startVoice (Voice& v, int note, float velocity, const Params& p, bool fresh)
    {
        v.active = true; v.note = note; v.velocity = velocity;
        v.curNote = v.glideFrom = v.glideTarget = note; v.glideLen = v.glideAge = 0;
        v.stage = Stage::Attack; v.startStamp = ++stamp;
        v.travel = 0.0; v.nextGrainIn = 0.0; v.age = 0;
        v.playPos = (double) std::clamp (p.position, 0.0f, 0.9999f) * (double) lastNumFrames;
        if (fresh) { v.env = 0.0f; for (auto& g : v.grains) g.active = false; }
    }

    void startGlide (Voice& v, double from, double to, const Params& p)
    {
        v.glideFrom = from;
        v.glideTarget = to;
        v.glideLen = std::max (1, (int) (p.portaMs * 0.001 * sr));
        v.glideAge = 0;
        v.curNote = from;
    }

    void glideTo (Voice& v, double to, const Params& p)
    {
        if (p.portaMs > 0.5f) startGlide (v, v.curNote, to, p);
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

    // ---- macros -> grain ----
    void spawnGrain (Voice& v, const Params& p, const SampleData& s, double baseRatio, double spacing, double fileRate, float bloom)
    {
        Grain* g = nullptr;
        for (auto& c : v.grains) if (! c.active) { g = &c; break; }
        if (g == nullptr) return;

        const int N = s.numFrames();
        const float smear = p.smear, scatter = p.scatter, drift = p.drift;

        // length: SIZE, stretched by BLOOM and SMEAR, breathing with DRIFT
        double holdMs = 12.0 * std::pow (80.0, (double) p.size);
        holdMs *= (1.0 + 1.5 * bloom) * (1.0 + 3.0 * smear * smear);
        holdMs *= 1.0 + 0.4 * drift * v.walk[2];
        holdMs = std::min (holdMs, 2500.0);
        const int length = std::max (32, (int) (holdMs * 0.001 * sr));

        // softness: SMEAR + BLOOM
        const float soft = std::clamp (0.15f + 0.85f * (smear * 0.9f + bloom * 0.5f), 0.0f, 1.0f);

        // pitch: SMEAR blur + DRIFT wander + SHIMMER octave/fifth sparkles
        double cents = (double) (smear * 25.0f * rng.bipolar()) + (double) (drift * 12.0f * v.walk[1]);
        uint8_t kind = 0;
        if (p.shimmer > 0.0f && rng.uni() < p.shimmer * 0.55f)
        {
            cents += rng.uni() < 0.35f ? 1900.0 : 1200.0;
            kind = 1;
        }
        bool reverse = p.shimmer > 0.0f && rng.uni() < p.shimmer * 0.3f;
        if (reverse && kind == 0) kind = 2;
        const double ratio = baseRatio * std::exp2 (cents / 1200.0);

        // position: POSITION + travel + SCATTER/SMEAR/BLOOM jitter + DRIFT wander
        const float jitter = scatter * 0.2f + smear * 0.12f + bloom * 0.08f;
        double start = (double) p.position + v.travel + (double) (jitter * rng.bipolar()) + (double) (drift * 0.12f * v.walk[0]);
        start = wrap (start, 1.0) * (double) N;

        // stereo: SCATTER + BLOOM + SMEAR, plus DRIFT pan wander
        const float width = std::clamp (scatter + bloom * 0.35f + smear * 0.2f, 0.0f, 1.0f);
        const float pan = std::clamp (width * rng.bipolar() + drift * 0.4f * v.walk[3], -1.0f, 1.0f);
        const float a = (pan + 1.0f) * 0.25f * (float) kPi;

        g->active = true;
        g->age = 0;
        g->length = length;
        g->fade = std::max (8, (int) (soft * (float) length * 0.5f));
        g->inc = reverse ? -ratio : ratio;
        g->pos = reverse ? start + ratio * (double) length : start;
        g->gainL = std::cos (a) * 1.41421356f;
        g->gainR = std::sin (a) * 1.41421356f;
        g->pan = pan;
        g->kind = kind;

        const double overlap = std::min ((double) kGrainsPerVoice, (double) length / std::max (1.0, spacing));
        v.norm = (float) (1.0 / std::sqrt (std::max (1.0, overlap * (0.5 + 0.5 * (1.0 - soft * 0.5)))));

        // tell the UI
        const uint32_t w = eventWrite.load (std::memory_order_relaxed);
        auto& e = events[(size_t) (w % kEventRing)];
        e.pos = (float) (start / (double) N);
        e.pan = pan;
        e.len = (float) (length / sr);
        e.kind = kind;
        eventWrite.store (w + 1, std::memory_order_release);
        (void) fileRate;
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
                ++n;
            }
        }
        vizCount.store (n, std::memory_order_release);
        vizHeadCount.store (h, std::memory_order_release);
    }

    double sr = 44100.0, driftCoef = 0.0001;
    std::array<Voice, kMaxVoices> voices {};
    std::array<int, 32> noteStack {};
    int stackSize = 0, heldCount = 0, lastNumFrames = 1;
    double lastPitch = -1.0;
    Random rng;
    uint64_t stamp = 0;
};

} // namespace granulum
