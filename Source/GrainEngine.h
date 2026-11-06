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
#include <utility>
#include <vector>

namespace granulum
{

static constexpr double kPi = 3.14159265358979323846;

struct SampleData
{
    std::vector<std::vector<float>> channels; // 1 or 2 channels
    double sampleRate = 44100.0;              // plays at "original pitch" on MIDI note 60
    // LFO on a harmonic: that harmonic lives in its own mono buffer (unit level, same length), mixed in at
    // play time with a gain that moves (Params::harmGain). layerHarm = which harmonic each layer is.
    std::vector<std::vector<float>> layers;
    std::vector<int> layerHarm;

    int numFrames() const   { return channels.empty() ? 0 : (int) channels[0].size(); }
    int numChannels() const { return (int) channels.size(); }
};

// ---------------------------------------------------------------------------
// Additive oscillator: 16 harmonics -> seamless looping buffer with slowly
// "breathing" partials. Levels are absolute: lower bars = quieter, all zero = silence.
static constexpr int kNumHarmonics = 16;

// Renders the harmonics split in two parts: those whose bit is set in grainMask (-> granulator)
// and the rest (-> clean). Both parts share the same loudness scaling, so grain + clean == full sound.
inline std::pair<SampleData, SampleData> renderAdditiveSplit (const std::array<float, kNumHarmonics>& amps, uint32_t grainMask, uint32_t layerMask = 0)
{
    constexpr int period = 184;           // samples per cycle
    constexpr int cycles = 1044;          // ~4 s
    constexpr int N = period * cycles;
    constexpr double c4 = 261.6255653;    // plays at true C4 on note 60

    SampleData g, c;
    g.sampleRate = c.sampleRate = period * c4;
    g.channels.assign (1, std::vector<float> ((size_t) N, 0.0f));
    c.channels.assign (1, std::vector<float> ((size_t) N, 0.0f));

    std::array<float, period> sine {};
    for (int i = 0; i < period; ++i)
        sine[(size_t) i] = (float) std::sin (2.0 * kPi * i / period);

    uint32_t seed = 0x1234567u;
    auto rnd = [&seed]() { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 16777216.0f; };

    for (int k = 1; k <= kNumHarmonics; ++k)
    {
        const float a = amps[(size_t) k - 1];
        const int    driftCycles = 1 + (int) (rnd() * 3.0f);
        const double driftPhase  = rnd() * 2.0 * kPi;
        const float  driftDepth  = (k == 1 ? 0.0f : 0.1f + 0.25f * rnd() * std::min (1.0f, (float) k / 4.0f));
        const int    phaseOff    = k == 1 ? 0 : (int) (rnd() * period);
        const bool layered = (layerMask >> (k - 1)) & 1u;
        if ((! layered && a <= 0.0005f) || k * c4 > 20000.0) continue;
        auto& part = ((grainMask >> (k - 1)) & 1u) ? g : c;
        std::vector<float>* out = &part.channels[0];
        float lev = a;
        if (layered)
        {
            part.layers.emplace_back ((size_t) N, 0.0f);
            part.layerHarm.push_back (k - 1);
            out = &part.layers.back();
            lev = 1.0f;     // unit level: the moving gain (the harmonic's own level + LFO) is applied at play time
        }
        for (int i = 0; i < N; ++i)
        {
            const float drift = 1.0f - driftDepth * 0.5f * (1.0f + (float) std::sin (2.0 * kPi * driftCycles * (double) i / N + driftPhase));
            (*out)[(size_t) i] += lev * drift * sine[(size_t) ((k * i + phaseOff) % period)];
        }
    }

    // level: from the mix at the harmonics' resting levels, so an LFO-free patch sounds exactly as before
    float peak = 0.0f;
    for (int i = 0; i < N; ++i)
    {
        float x = g.channels[0][(size_t) i] + c.channels[0][(size_t) i];
        for (auto* part : { &g, &c })
            for (size_t j = 0; j < part->layers.size(); ++j)
                x += amps[(size_t) part->layerHarm[j]] * part->layers[j][(size_t) i];
        peak = std::max (peak, std::abs (x) * 0.7f);
    }
    const float gain = 0.7f * (peak > 0.95f ? 0.95f / peak : 1.0f);
    for (auto* part : { &g, &c })
    {
        for (auto& v : part->channels[0]) v *= gain;
        for (auto& L : part->layers) for (auto& v : L) v *= gain;
    }
    return { std::move (g), std::move (c) };
}

inline SampleData renderAdditive (const std::array<float, kNumHarmonics>& amps)
{
    return renderAdditiveSplit (amps, 0xffffu).first;
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
    float chime    = 0.0f;   // CHIME: grains ring a bank of resonators tuned to the note
    float spread   = 1.0f;   // SPREAD: stereo width of the grain cloud (0 = all grains centred)
    // per-harmonic level for layered harmonics (LFO on a stick): now and at the previous chunk (ramped in between)
    std::array<float, kNumHarmonics> harmGain {}, harmGainPrev {};
    bool  grainOn  = false;

    // LOOP: grains (and the plain playback) stay inside [loopA, loopB] of the source
    int   loopMode = 0;      // 0 OFF (whole file), 1 LOOP, 2 PING-PONG, 3 ONE-SHOT
    float loopA = 0.0f, loopB = 1.0f;

    // INTO GRAINS: LFOs whose rhythm on this OSC's volume is cut up by the grains. Each grain carries its own
    // piece of the LFO rhythm, taken from a slightly different moment (more SCATTER = further apart).
    struct GrainLfo { const float* table = nullptr; int size = 0; double phase = 0.0, inc = 0.0; float depth = 0.0f, rise = 1.0f; };
    std::array<GrainLfo, 3> glfo {};
    int   glfoCount = 0;
    bool  volInGrains = false;     // OSC volume is applied here instead of after the engine
    float volBase = 1.0f;          // normalised volume without the INTO GRAINS LFOs
    float volScale = 1.0f;         // OSC 3 is a little louder
    float level = 1.0f, levelPrev = 1.0f;   // full volume now / at the previous chunk (dry part)

    // oscillator
    float transpose = 0.0f;  // semitones
    bool  keyTrack  = true;  // false: every note plays the source at its original pitch (noise)
    bool  randomStart = false; // voices start at random places in the source (noise)
    std::array<int, 5> chord { 0, 0, 0, 0, 0 };
    int chordSize = 1;

    // AMP envelope (ADSR)
    float attackMs  = 5.0f;
    float decayMs   = 300.0f;
    float sustain   = 1.0f;
    float releaseMs = 300.0f;

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
    bool   perc = false; // SHIMMER "ping": sharp attack, ringing decay (glass / bells)
    double lph[3] {};    // INTO GRAINS: this grain's own position in each LFO's rhythm
    float  gvs = -1.0f;  // INTO GRAINS: smoothed volume (no clicks on hard LFO edges)
};

// A grain that was just born — the UI turns these into flying shards.
struct GrainEvent
{
    float pos = 0.0f;    // 0..1 in the source
    float pan = 0.0f;    // -1..1
    float len = 0.0f;    // seconds
    uint8_t kind = 0;
};

// CHIME resonator bank: ratios morph from harmonic (glassy) to struck-bar (bell) partials
static constexpr int kChimeModes = 6;
static constexpr float kChimeHarm[kChimeModes] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };
static constexpr float kChimeBell[kChimeModes] = { 1.0f, 2.756f, 5.404f, 8.933f, 13.34f, 18.64f };
static constexpr float kChimePan[kChimeModes]  = { -0.15f, 0.55f, -0.7f, 0.8f, -0.5f, 0.35f };

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
    std::atomic<float> vizEnv { 0.0f };      // AMP envelope level of the newest voice (UI dot)
    std::atomic<int>   vizStage { 0 };       // 0 off, 1 attack, 2 decay, 3 sustain, 4 release

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

        // layered harmonics (LFO on a stick): their buffers + gain ramp over this block
        const int M = hasSample ? std::min ((int) sample->layers.size(), kNumHarmonics) : 0;
        std::array<const float*, kNumHarmonics> lay {};
        std::array<float, kNumHarmonics> lg0 {}, lgStep {};
        for (int j = 0; j < M; ++j)
        {
            const int hk = std::clamp (sample->layerHarm[(size_t) j], 0, kNumHarmonics - 1);
            lay[(size_t) j] = sample->layers[(size_t) j].data();
            lg0[(size_t) j] = p.harmGainPrev[(size_t) hk];
            lgStep[(size_t) j] = (p.harmGain[(size_t) hk] - p.harmGainPrev[(size_t) hk]) / (float) std::max (1, numSamples);
        }
        // reads the source + the layers at pos (mono layers are added to both sides)
        auto readAll = [&] (double pos, int i, float& l, float& r)
        {
            readHermite (srcL, srcR, N, pos, l, r);
            if (M == 0) return;
            // layers are pure sines (one harmonic each, 184+ samples per cycle at most 16x): linear reading is clean enough
            // and much cheaper than the Hermite read used for the main buffer
            double q = pos;
            if (q < 0.0 || q >= (double) N) q = wrap (q, (double) N);
            const int i0 = std::min (N - 1, (int) q);
            const int i1 = i0 + 1 >= N ? 0 : i0 + 1;
            const float fr = (float) (q - (double) i0);
            float x = 0.0f;
            for (int j = 0; j < M; ++j)
            {
                const float* Lj = lay[(size_t) j];
                x += (Lj[i0] + (Lj[i1] - Lj[i0]) * fr) * (lg0[(size_t) j] + lgStep[(size_t) j] * (float) (i + 1));
            }
            l += x; r += x;
        };

        const float attackInc  = 1.0f / (std::max (0.0005f, p.attackMs * 0.001f) * (float) sr);
        const float sus        = std::clamp (p.sustain, 0.0f, 1.0f);
        const float decayDec   = (1.0f - sus) / (std::max (0.001f, p.decayMs * 0.001f) * (float) sr);
        const float releaseSec = std::max (p.releaseMs * 0.001f, p.grainOn ? 0.15f + 1.2f * p.chime * p.chime : 0.0f);
        const float releaseCoef = (float) std::exp (-6.907755 / (std::max (0.003f, releaseSec) * sr));   // -60 dB over RELEASE

        const bool chimeOn = p.grainOn && p.chime > 0.001f;
        const float strikeDecay = (float) std::exp (-1.0 / (0.0015 * sr));
        const float gvSlew = 1.0f - (float) std::exp (-1.0 / (0.0015 * sr));
        const float chimeT   = std::clamp ((p.chime - 0.55f) / 0.45f, 0.0f, 1.0f); // harmonic -> bell
        const float chimeT60 = 0.08f + 2.2f * p.chime * p.chime;                    // ring time (s)
        const float chimeAmt = 3.6f * std::pow (p.chime, 1.3f);

        for (auto& v : voices)
        {
            if (! v.active) continue;

            if (chimeOn)
            {
                // the whole bank is lifted by octaves into the "air" (>= 700 Hz): it stays in key
                // but never piles up in the low mids, even on low chords
                const double noteNow = (p.keyTrack ? v.curNote : 60.0) + (double) p.transpose;
                double f0 = 261.6255653 * std::exp2 ((noteNow - 60.0) / 12.0);
                while (f0 < 700.0) f0 *= 2.0;
                const float voiceTrim = std::sqrt (v.chordGain);          // calmer per note on big chords
                for (int m = 0; m < kChimeModes; ++m)
                {
                    const double ratio = kChimeHarm[m] + (kChimeBell[m] - kChimeHarm[m]) * chimeT;
                    const double f = f0 * ratio;
                    auto& c = v.chimeCoef[(size_t) m];
                    if (f > std::min (12000.0, sr * 0.45)) { c.gain = 0.0f; continue; }
                    const double t60 = chimeT60 / std::pow (ratio, 0.35);
                    const double r = std::exp (-6.907755 / (t60 * sr));
                    const double detune = 1.0 + 0.0012 * (m + 1);                 // L/R shimmer
                    c.a1L = (float) (2.0 * r * std::cos (2.0 * kPi * f / sr));
                    c.a1R = (float) (2.0 * r * std::cos (2.0 * kPi * f * detune / sr));
                    c.a2 = (float) (-r * r);
                    c.gain = (float) ((1.0 - r) * 2.0 / std::pow ((double) (m + 1), 0.15)) * voiceTrim;
                    const float pan = kChimePan[m];
                    c.gl = std::sqrt (0.5f * (1.0f - pan)) * 1.41421356f;
                    c.gr = std::sqrt (0.5f * (1.0f + pan)) * 1.41421356f;
                }
            }

            for (int i = 0; i < numSamples; ++i)
            {
                if (v.stage == Stage::Attack)
                {
                    v.env += attackInc;
                    if (v.env >= 1.0f) { v.env = 1.0f; v.stage = Stage::Decay; }
                }
                else if (v.stage == Stage::Decay)
                {
                    v.env -= decayDec;
                    if (v.env <= sus) { v.env = sus; v.stage = Stage::Sustain; }
                }
                else if (v.stage == Stage::Sustain)
                {
                    v.env += (sus - v.env) * 0.002f;   // follows SUSTAIN live, without clicks
                }
                else if (v.stage == Stage::Release)
                {
                    v.env *= releaseCoef;
                    if (v.env <= 0.001f)
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

                const double semis = (p.keyTrack ? v.curNote - 60.0 : 0.0) + (double) p.transpose;
                const double ratio = std::exp2 (semis / 12.0) * fileRate;
                const float  amp   = v.env * v.velocity * v.chordGain;

                // dry: plain looping playback
                float dl, dr;
                readAll (v.playPos, i, dl, dr);
                if (p.loopMode == 0)
                {
                    v.playPos += ratio;
                    if (v.playPos >= (double) N) v.playPos -= (double) N;
                }
                else
                {
                    const double A = (double) p.loopA * N, B = std::max (A + 64.0, (double) p.loopB * N);
                    if (v.playDone) { dl = dr = 0.0f; }
                    else
                    {
                        v.playPos += ratio * (double) v.playDir;
                        if (p.loopMode == 1)      { if (v.playPos >= B || v.playPos < A) v.playPos = A + std::fmod (std::max (0.0, v.playPos - A), B - A); }
                        else if (p.loopMode == 2) { if (v.playPos >= B) { v.playPos = B - (v.playPos - B); v.playDir = -1; }
                                                    else if (v.playPos < A) { v.playPos = A + (A - v.playPos); v.playDir = 1; } }
                        else if (v.playPos >= B)  { v.playDone = true; }
                    }
                }

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
                    const double loopLen = p.loopMode == 0 ? 1.0 : std::max (0.005, (double) (p.loopB - p.loopA));
                    v.travel += (double) p.speed * spacing * fileRate / (double) N / loopLen;   // speed stays "seconds of audio"
                    if (p.loopMode == 3) v.travel = std::clamp (v.travel, -1.0, 1.0);           // ONE-SHOT: stops at the end
                    else if (p.loopMode == 2) v.travel -= 2.0 * std::floor (v.travel * 0.5);
                    else v.travel -= std::floor (v.travel);
                    spacing *= 1.0 + (double) p.scatter * 0.6 * (double) rng.bipolar();
                    // high SCATTER breaks the stream: quick bursts and holes of silence
                    const float brk = std::clamp ((p.scatter - 0.4f) / 0.6f, 0.0f, 1.0f);
                    if (brk > 0.0f)
                    {
                        const float u = rng.uni();
                        if (u < 0.25f * brk)       spacing *= 1.8 + 4.5 * (double) rng.uni();   // hole
                        else if (u < 0.60f * brk)  spacing *= 0.18 + 0.2 * (double) rng.uni();  // burst
                    }
                    v.nextGrainIn += std::max (8.0, spacing);
                }

                float sumL = 0.0f, sumR = 0.0f;
                for (auto& g : v.grains)
                {
                    if (! g.active) continue;
                    const float w = g.perc ? percWindow (g.age, g.length) * 1.6f : window (g.age, g.length, g.fade);
                    float l, r;
                    readAll (g.pos, i, l, r);
                    float gv = 1.0f;
                    if (p.volInGrains)
                    {
                        float vn = p.volBase;
                        for (int k = 0; k < p.glfoCount; ++k)
                        {
                            const auto& L = p.glfo[(size_t) k];
                            const double ph = g.lph[k] - std::floor (g.lph[k]);
                            const double x = ph * (double) L.size;
                            const int i0 = std::min (L.size - 1, (int) x);
                            const float f = (float) (x - (double) i0);
                            vn += L.depth * L.rise * (L.table[i0] + (L.table[i0 + 1] - L.table[i0]) * f);
                            g.lph[k] += L.inc;
                        }
                        // ~1.5 ms smoothing: square / hard-edged LFOs at high rates no longer click inside the grains
                        const float target = std::clamp (vn, 0.0f, 1.0f) * p.volScale;
                        g.gvs = g.gvs < 0.0f ? target : g.gvs + (target - g.gvs) * gvSlew;
                        gv = g.gvs;
                    }
                    sumL += l * w * g.gainL * gv;
                    sumR += r * w * g.gainR * gv;
                    g.pos += g.inc;
                    if (++g.age >= g.length) g.active = false;
                }

                const float wet = p.mix * ev * ev;
                const float ga = v.norm * 1.2f;
                float gl = sumL * ga, gr = sumR * ga;
                if (chimeOn)
                {
                    // hammer: a 1-2 ms noise click that rings the resonators even on smooth pads
                    const float strikeNow = v.strike * rng.bipolar() * (0.4f + 1.6f * p.chime);
                    v.strike *= strikeDecay;
                    float rl = 0.0f, rr = 0.0f;
                    for (int m = 0; m < kChimeModes; ++m)
                    {
                        const auto& c = v.chimeCoef[(size_t) m];
                        if (c.gain == 0.0f) continue;
                        auto& st = v.chimeState[(size_t) m];
                        const float ex = 0.5f * (gl + gr) + strikeNow;
                        const float yl = c.gain * ex + c.a1L * st[0] + c.a2 * st[1];
                        const float yr = c.gain * ex + c.a1R * st[2] + c.a2 * st[3];
                        st[1] = st[0]; st[0] = yl;
                        st[3] = st[2]; st[2] = yr;
                        rl += yl * c.gl; rr += yr * c.gr;
                    }
                    gl += rl * chimeAmt;
                    gr += rr * chimeAmt;
                }
                const float dryLevel = p.volInGrains ? p.levelPrev + (p.level - p.levelPrev) * (float) (i + 1) / (float) numSamples : 1.0f;
                outL[i] += (dl * 0.85f * (1.0f - wet) * dryLevel + gl * wet) * amp;
                outR[i] += (dr * 0.85f * (1.0f - wet) * dryLevel + gr * wet) * amp;
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

    // percussive grain: 2 ms attack, exponential decay over the grain
    inline float percWindow (int age, int length) const
    {
        const float att = std::min (1.0f, (float) (age + 1) / (float) (0.002 * sr));
        const float t = (float) age / (float) std::max (1, length);
        return att * std::exp (-5.0f * t) * std::min (1.0f, (float) (length - age) / 64.0f);
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
    enum class Stage { Off, Attack, Decay, Sustain, Release };

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
        float    strike = 0.0f;       // CHIME hammer: a short click every time a grain is born
        double   playPos = 0.0;
        int      playDir = 1;            // PING-PONG direction of the plain playback
        bool     playDone = false;       // ONE-SHOT plain playback reached the end
        int64_t  age = 0;
        uint64_t startStamp = 0;
        float    walk[4] {}, walkTarget[4] {};
        bool     panSide = false;   // grains alternate left / right, so the cloud never sits in one ear
        int      walkCount[4] {};
        struct ChimeCoef { float a1L = 0, a1R = 0, a2 = 0, gain = 0, gl = 1, gr = 1; };
        std::array<ChimeCoef, kChimeModes> chimeCoef {};
        std::array<std::array<float, 4>, kChimeModes> chimeState {};
        std::array<Grain, kGrainsPerVoice> grains {};
    };

    void startVoice (Voice& v, int note, float velocity, const Params& p, bool fresh)
    {
        v.active = true; v.note = note; v.velocity = velocity;
        v.curNote = v.glideFrom = v.glideTarget = note; v.glideLen = v.glideAge = 0;
        v.stage = Stage::Attack; v.startStamp = ++stamp;
        v.travel = 0.0; v.nextGrainIn = 0.0; v.age = 0;
        v.playPos = loopHead ((double) std::clamp (p.position, 0.0f, 0.9999f), p) * (double) lastNumFrames;
        v.playDir = 1; v.playDone = false;
        if (p.randomStart)
        {
            v.travel = rng.uni();
            v.playPos = std::fmod (v.playPos + v.travel * (double) lastNumFrames, (double) lastNumFrames);
            if (p.loopMode != 0) v.playPos = inLoop (v.playPos / (double) lastNumFrames, p) * (double) lastNumFrames;
        }
        if (fresh)
        {
            v.env = 0.0f;
            for (auto& g : v.grains) g.active = false;
            for (auto& st : v.chimeState) st = {};
        }
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

    // where the read head is (0..1 of the file) for "position + travel" under the LOOP mode
    static inline double loopHead (double u, const Params& p)
    {
        const double a = p.loopA, b = std::max ((double) p.loopA + 0.005, (double) p.loopB);
        switch (p.loopMode)
        {
            case 1:  return a + wrap (u, 1.0) * (b - a);
            case 2:  { const double f = wrap (u, 2.0); return a + (f < 1.0 ? f : 2.0 - f) * (b - a); }
            case 3:  return a + std::clamp (u, 0.0, 1.0) * (b - a);
            default: return wrap (u, 1.0);
        }
    }
    // keep a grain start inside the loop region
    static inline double inLoop (double x, const Params& p)
    {
        if (p.loopMode == 0) return wrap (x, 1.0);
        const double a = p.loopA, b = std::max ((double) p.loopA + 0.005, (double) p.loopB);
        return a + wrap ((x - a) / (b - a), 1.0) * (b - a);
    }

    static inline double wrap (double x, double n)
    {
        x = std::fmod (x, n);
        return x < 0.0 ? x + n : x;
    }

    static inline void readHermite (const float* L, const float* R, int N, double pos, float& outL, float& outR)
    {
        if (pos < 0.0 || pos >= (double) N) pos = wrap (pos, (double) N);   // fast path: almost always inside already
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
        int length = std::max (32, (int) (holdMs * 0.001 * sr));

        // softness: SMEAR + BLOOM
        const float soft = std::clamp (0.15f + 0.85f * (smear * 0.9f + bloom * 0.5f), 0.0f, 1.0f);

        // pitch: SMEAR blur + DRIFT wander + SHIMMER octave/fifth sparkles
        double cents = (double) (smear * 25.0f * rng.bipolar()) + (double) (drift * 12.0f * v.walk[1]);
        uint8_t kind = 0;
        bool perc = false;
        if (p.shimmer > 0.0f && rng.uni() < p.shimmer * 0.6f)
        {
            // more SHIMMER = higher, glassier jumps (always in key: octaves, fifths, a third)
            const float hi = p.shimmer, u = rng.uni();
            if      (u < 0.06f * hi)        cents -= 1200.0;                 // rare low drop
            else if (u < 0.40f - 0.15f * hi) cents += 1200.0;
            else if (u < 0.62f - 0.10f * hi) cents += 1900.0;
            else if (u < 0.85f)              cents += 2400.0;
            else                             cents += rng.uni() < 0.5f ? 2786.0 : 3100.0;
            kind = 1;
            // above ~40% part of the sparkles become short "pings": bells / glass
            perc = rng.uni() < std::clamp ((hi - 0.35f) / 0.65f, 0.0f, 1.0f) * 0.7f;
        }
        bool reverse = p.shimmer > 0.0f && rng.uni() < p.shimmer * 0.3f;
        if (reverse && kind == 0) kind = 2;
        const double ratio = baseRatio * std::exp2 (cents / 1200.0);

        // position: POSITION + travel + SCATTER/SMEAR/BLOOM jitter + DRIFT wander
        const float jitter = scatter * 0.15f + scatter * scatter * 0.45f + smear * 0.12f + bloom * 0.08f;
        const double region = p.loopMode == 0 ? 1.0 : std::max (0.005, (double) (p.loopB - p.loopA));
        double start = loopHead ((double) p.position + v.travel, p) + ((double) (jitter * rng.bipolar()) + (double) (drift * 0.12f * v.walk[0])) * region;
        start = inLoop (start, p) * (double) N;

        // stereo: SCATTER + BLOOM + SMEAR (+ DRIFT sway) set how far a grain goes from the centre, SPREAD scales it.
        // Grains take turns left / right, so the cloud stays centred on average (DRIFT used to push the whole cloud
        // into one ear for seconds at a time).
        const float width = std::clamp (scatter + bloom * 0.35f + smear * 0.2f, 0.0f, 1.0f);
        const float away = std::abs (width * rng.bipolar()) + drift * 0.4f * std::abs (v.walk[3]);
        v.panSide = ! v.panSide;
        const float pan = std::clamp ((v.panSide ? away : -away) * std::clamp (p.spread, 0.0f, 1.0f), -1.0f, 1.0f);
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
        g->perc = perc;
        g->gvs = -1.0f;
        if (p.volInGrains)
        {
            // this grain's slice of the LFO rhythm comes from a moment up to ~1 s back (SCATTER), or right now
            const double back = (0.02 + 0.98 * (double) scatter) * sr * (double) rng.uni();
            for (int k = 0; k < p.glfoCount; ++k)
            {
                const double ph = p.glfo[(size_t) k].phase - back * p.glfo[(size_t) k].inc;
                g->lph[k] = ph - std::floor (ph);
            }
        }
        if (perc)
        {
            g->length = length = std::max (64, (int) ((0.06 + 0.35 * rng.uni()) * sr));
            g->pos = reverse ? start + ratio * (double) length : start;
        }
        // CHIME hammer: every grain strikes the bells (pings and sharp grains hit harder)
        v.strike = std::max (v.strike, perc ? 1.0f : 0.25f + 0.75f * (1.0f - soft));

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
        {
            const Voice* newest = nullptr;
            for (auto& v : voices) if (v.active && (newest == nullptr || v.startStamp > newest->startStamp)) newest = &v;
            vizEnv.store (newest ? newest->env : 0.0f, std::memory_order_relaxed);
            vizStage.store (newest ? (int) newest->stage : 0, std::memory_order_relaxed);
        }
        int n = 0, h = 0;
        for (auto& v : voices)
        {
            if (! v.active || ! hasSample) continue;
            if (h < kMaxVoices)
            {
                const double head = p.grainOn ? loopHead ((double) p.position + v.travel, p) : v.playPos / (double) N;
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
