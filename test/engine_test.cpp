// Offline test of the granular engine: no NaNs, sane levels, voices finish.
// Also writes short WAV renders for listening checks.
#include "../Source/GrainEngine.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>

using namespace granulum;

static SampleData makeDrumLoop()
{
    SampleData s;
    s.sampleRate = 48000.0;
    const int N = 48000 * 2;
    s.channels.assign (2, std::vector<float> ((size_t) N, 0.0f));
    Random r (5);
    for (int beat = 0; beat < 8; ++beat)
    {
        const int at = beat * N / 8;
        const bool kick = beat % 2 == 0;
        for (int i = 0; i < 9000 && at + i < N; ++i)
        {
            const double t = i / 48000.0;
            const float v = kick ? (float) (std::sin (2 * kPi * (50 + 120 * std::exp (-t * 30)) * t) * std::exp (-t * 12))
                                 : (float) (r.bipolar() * std::exp (-t * 25) * 0.7);
            s.channels[0][(size_t) (at + i)] += v;
            s.channels[1][(size_t) (at + i)] += v;
        }
    }
    // a pad under it
    for (int i = 0; i < N; ++i)
    {
        const double t = i / 48000.0;
        s.channels[0][(size_t) i] += (float) (0.15 * std::sin (2 * kPi * 220 * t));
        s.channels[1][(size_t) i] += (float) (0.15 * std::sin (2 * kPi * 277 * t));
    }
    s.transients = detectTransients (s);
    return s;
}

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    const uint32_t n = (uint32_t) L.size(), bytes = n * 4;
    auto w32 = [f] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [f] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + bytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (bytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        auto c = [] (float x) { return (int16_t) std::lrint (std::max (-1.0f, std::min (1.0f, x)) * 32000.0f); };
        int16_t a = c (L[i]), b = c (R[i]);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

static bool run (const char* name, Params p, int note, double seconds, double releaseAt, const SampleData& s, const char* wavDir)
{
    GrainEngine e;
    e.prepare (44100.0);
    e.noteOn (note, 0.9f, p.mono);

    const int block = 256;
    const int total = (int) (seconds * 44100);
    std::vector<float> L (block), R (block), S (block), allL, allR;
    double peak = 0, sumSq = 0;
    long count = 0;
    bool released = false;
    for (int pos = 0; pos < total; pos += block)
    {
        if (! released && pos >= releaseAt * 44100) { e.noteOff (note, p.mono); released = true; }
        std::fill (L.begin(), L.end(), 0.0f);
        std::fill (R.begin(), R.end(), 0.0f);
        std::fill (S.begin(), S.end(), 0.0f);
        e.render (L.data(), R.data(), S.data(), block, p, &s);
        for (int i = 0; i < block; ++i) { L[(size_t) i] += S[(size_t) i]; R[(size_t) i] += S[(size_t) i]; }
        for (int i = 0; i < block; ++i)
        {
            if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (R[(size_t) i])) { printf ("%s: NaN!\n", name); return false; }
            peak = std::max (peak, (double) std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i])));
            sumSq += L[(size_t) i] * L[(size_t) i];
            ++count;
            allL.push_back (std::tanh (L[(size_t) i] * 0.7f));
            allR.push_back (std::tanh (R[(size_t) i] * 0.7f));
        }
    }
    if (wavDir) writeWav (std::string (wavDir) + "/" + name + ".wav", allL, allR, 44100);
    const double rms = std::sqrt (sumSq / count);
    const int active = e.activeVoiceCount();
    printf ("%-12s peak=%.3f rms=%.3f voicesLeft=%d\n", name, peak, rms, active);
    return peak > 0.01 && peak < 6.0 && active == 0;
}

int main (int argc, char** argv)
{
    const char* wavDir = argc > 1 ? argv[1] : nullptr;
    const auto s = makeDrumLoop();
    printf ("transients detected: %zu (expected ~8)\n", s.transients.size());

    bool ok = true;
    Params p;
    ok &= run ("default", p, 60, 3.0, 2.5, s, wavDir);

    Params buzz = p; buzz.attack = 0.0f; buzz.holdMs = 4; buzz.spacingMs = 3; buzz.waveSpacing = 0.4f;
    ok &= run ("buzz", buzz, 60, 3.0, 2.5, s, wavDir);

    Params stutter = p; stutter.attack = 0.05f; stutter.holdMs = 40; stutter.spacingMs = 120; stutter.waveSpacing = 1.0f; stutter.pan = 1.0f;
    ok &= run ("stutter", stutter, 60, 3.0, 2.5, s, wavDir);

    Params smear = p; smear.smear = 1.0f; smear.waveSpacing = 0.3f;
    ok &= run ("smear", smear, 60, 3.0, 2.5, s, wavDir);

    Params stretch = p; stretch.attack = 0.8f; stretch.holdMs = 200; stretch.spacingMs = 50; stretch.waveSpacing = 0.25f; stretch.transients = true;
    ok &= run ("stretch_tr", stretch, 60, 4.0, 3.5, s, wavDir);

    Params warp = p; warp.waveSpacing = -1.5f; warp.lfoDepth = 0.6f; warp.lfoRate = 2.0f; warp.rand = 0.4f; warp.octaveProb = 0.4f; warp.reverseProb = 0.5f;
    ok &= run ("warp", warp, 48, 3.0, 2.5, s, wavDir);

    Params dense = p; dense.holdMs = 1000; dense.spacingMs = 1; dense.attack = 1.0f;
    ok &= run ("dense", dense, 72, 3.0, 2.0, s, wavDir);

    // additive source + key sync bass on A1 (note 33): output should be pitched at 55 Hz
    std::array<float, kNumHarmonics> h {}; h[0] = 1; h[1] = 0.6f; h[2] = 0.4f; h[4] = 0.3f;
    const auto add = renderAdditive (h);
    printf ("additive frames=%d sr=%.1f\n", add.numFrames(), add.sampleRate);
    Params bass = p; bass.keySync = true; bass.mono = true; bass.sub = 0.5f; bass.holdMs = 60; bass.attack = 0.3f;
    ok &= run ("keysync_A1", bass, 33, 3.0, 2.5, add, wavDir);
    Params vox = bass; vox.sub = 0.0f; vox.pitch = 7.0f; vox.waveSpacing = 0.3f;
    ok &= run ("keysync_drum", vox, 33, 3.0, 2.5, s, wavDir);

    GrainEngine e; e.prepare (48000);
    for (int n = 0; n < 9; ++n) e.noteOn (48 + n, 1.0f, false);
    printf ("steal: voices=%d\n", e.activeVoiceCount());

    printf (ok ? "ALL OK\n" : "FAILED\n");
    return ok ? 0 : 1;
}
