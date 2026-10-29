// Offline test of the granular engine: no NaNs, sane levels, voices finish.
#include "../Source/GrainEngine.h"
#include <cstdio>
#include <cmath>

using namespace granulum;

static bool run (const char* name, Params p, int note, double seconds, double releaseAt)
{
    SampleData s;
    s.sampleRate = 48000.0;
    const int N = 48000 * 3;
    s.channels.assign (2, std::vector<float> ((size_t) N));
    for (int i = 0; i < N; ++i)
    {
        const double t = i / 48000.0;
        s.channels[0][(size_t) i] = (float) (0.6 * std::sin (2 * kPi * 220 * t) * std::exp (-((i % 24000) / 12000.0)));
        s.channels[1][(size_t) i] = (float) (0.6 * std::sin (2 * kPi * 330 * t));
    }

    GrainEngine e;
    e.prepare (44100.0);
    e.noteOn (note, 0.9f);

    const int block = 256;
    const int total = (int) (seconds * 44100);
    std::vector<float> L (block), R (block);
    double peak = 0, sumSq = 0;
    long count = 0;
    bool released = false;
    for (int pos = 0; pos < total; pos += block)
    {
        if (! released && pos >= releaseAt * 44100) { e.noteOff (note); released = true; }
        std::fill (L.begin(), L.end(), 0.0f);
        std::fill (R.begin(), R.end(), 0.0f);
        e.render (L.data(), R.data(), block, p, &s);
        for (int i = 0; i < block; ++i)
        {
            if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (R[(size_t) i])) { printf ("%s: NaN!\n", name); return false; }
            peak = std::max (peak, (double) std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i])));
            sumSq += L[(size_t) i] * L[(size_t) i];
            ++count;
        }
    }
    const double rms = std::sqrt (sumSq / count);
    const int active = e.activeVoiceCount();
    printf ("%-14s peak=%.3f rms=%.3f voicesLeft=%d\n", name, peak, rms, active);
    return peak > 0.01 && peak < 4.0 && active == 0;
}

int main()
{
    bool ok = true;
    Params p;
    ok &= run ("default", p, 60, 3.0, 1.5);

    Params dense = p; dense.density = 200; dense.sizeMs = 2000; dense.chaos = 1;
    ok &= run ("dense", dense, 72, 5.0, 2.0);

    Params wild = p; wild.scan = -2; wild.pitchSpray = 12; wild.octaveProb = 1; wild.reverseProb = 0.5f; wild.shape = 1; wild.spray = 1;
    ok &= run ("wild", wild, 36, 3.0, 1.0);

    Params tiny = p; tiny.sizeMs = 5; tiny.density = 1; tiny.releaseMs = 5;
    ok &= run ("tiny", tiny, 60, 2.0, 1.0);

    // window must start and end near zero for smooth shapes
    for (float sh : { 0.0f, 0.5f, 1.0f })
        if (GrainEngine::window (0.0f, sh) > 0.01f || GrainEngine::window (1.0f, sh) > 0.01f) { printf ("window edge fail %.1f\n", sh); ok = false; }

    // 9 notes on 8 voices -> stealing must not crash
    GrainEngine e; e.prepare (48000);
    for (int n = 0; n < 9; ++n) e.noteOn (48 + n, 1.0f);
    printf ("steal: voices=%d\n", e.activeVoiceCount());

    printf (ok ? "ALL OK\n" : "FAILED\n");
    return ok ? 0 : 1;
}
