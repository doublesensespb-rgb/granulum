// Offline test of the granular engine: no NaNs, sane levels, voices finish, pitch/porta correct.
#include "../Source/GrainEngine.h"
#include <cstdio>
#include <cmath>
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
    s.transients = detectTransients (s);
    return s;
}

struct Result { double peak = 0, rms = 0; int voicesLeft = 0; bool ok = true; std::vector<float> L; };

static Result run (Params p, std::vector<std::pair<double, int>> events, double seconds, const SampleData& s)
{
    // events: (time s, +note on / -note off)
    GrainEngine e;
    e.prepare (44100.0);
    Result res;
    const int block = 64;
    const int total = (int) (seconds * 44100);
    std::vector<float> L (block), R (block);
    double sumSq = 0;
    size_t ev = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        while (ev < events.size() && events[ev].first * 44100 <= pos)
        {
            const int n = events[ev].second;
            if (n > 0) e.noteOn (n, 0.9f, p); else e.noteOff (-n, p);
            ++ev;
        }
        std::fill (L.begin(), L.end(), 0.0f);
        std::fill (R.begin(), R.end(), 0.0f);
        e.render (L.data(), R.data(), block, p, &s);
        for (int i = 0; i < block; ++i)
        {
            if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (R[(size_t) i])) { res.ok = false; return res; }
            res.peak = std::max (res.peak, (double) std::abs (L[(size_t) i]));
            sumSq += L[(size_t) i] * L[(size_t) i];
            res.L.push_back (L[(size_t) i]);
        }
    }
    res.rms = std::sqrt (sumSq / (double) res.L.size());
    res.voicesLeft = e.activeVoiceCount();
    return res;
}

// zero-crossing based frequency estimate in a window
static double freqAt (const std::vector<float>& x, double t0, double t1)
{
    const int a = (int) (t0 * 44100), b = (int) (t1 * 44100);
    int crossings = 0;
    for (int i = a + 1; i < b && i < (int) x.size(); ++i)
        if (x[(size_t) i - 1] <= 0.0f && x[(size_t) i] > 0.0f) ++crossings;
    return crossings / (t1 - t0);
}

int main()
{
    bool ok = true;
    const auto drums = makeDrumLoop();
    printf ("transients: %zu (expect ~8)\n", drums.transients.size());

    // additive: single bar 1 = pure sine. Plain mode on A1 must be 55 Hz.
    std::array<float, kNumHarmonics> h {}; h[0] = 1.0f;
    const auto sine = renderAdditive (h);
    Params clean; clean.grainOn = false;
    auto r1 = run (clean, { { 0.0, 33 }, { 1.5, -33 } }, 2.0, sine);
    const double f1 = freqAt (r1.L, 0.3, 1.3);
    printf ("clean sine A1: %.1f Hz (expect 55), peak %.2f, left %d\n", f1, r1.peak, r1.voicesLeft);
    ok &= r1.ok && std::abs (f1 - 55.0) < 1.5 && r1.voicesLeft == 0;

    // transpose +12 -> 110 Hz
    Params up = clean; up.transpose = 12.0f;
    auto r2 = run (up, { { 0.0, 33 }, { 1.5, -33 } }, 2.0, sine);
    const double f2 = freqAt (r2.L, 0.3, 1.3);
    printf ("transpose +12: %.1f Hz (expect 110)\n", f2);
    ok &= std::abs (f2 - 110.0) < 2.0;

    // all bars down -> silence
    std::array<float, kNumHarmonics> zero {};
    auto r3 = run (clean, { { 0.0, 33 }, { 0.5, -33 } }, 1.0, renderAdditive (zero));
    printf ("all bars zero: peak %.5f (expect 0)\n", r3.peak);
    ok &= r3.peak < 1.0e-6;

    // lower bar = quieter (absolute levels)
    std::array<float, kNumHarmonics> half {}; half[0] = 0.5f;
    auto r4 = run (clean, { { 0.0, 33 }, { 1.0, -33 } }, 1.2, renderAdditive (half));
    printf ("bar at 50%%: peak %.2f vs full %.2f\n", r4.peak, r1.peak);
    ok &= r4.peak < r1.peak * 0.6;

    // mono legato porta: A1 -> A2 over 200 ms, middle of glide in between
    Params porta = clean; porta.mono = true; porta.portaMs = 200.0f;
    auto r5 = run (porta, { { 0.0, 33 }, { 0.5, 45 }, { 1.5, -45 }, { 1.5, -33 } }, 2.0, sine);
    const double fStart = freqAt (r5.L, 0.2, 0.48), fMid = freqAt (r5.L, 0.56, 0.64), fEnd = freqAt (r5.L, 0.9, 1.4);
    printf ("porta legato: %.1f -> %.1f -> %.1f Hz (expect 55 -> ~78 -> 110)\n", fStart, fMid, fEnd);
    ok &= std::abs (fStart - 55) < 4 && fMid > 62 && fMid < 100 && std::abs (fEnd - 110) < 3;

    // poly without ALWAYS: separate notes do not glide
    Params poly = clean; poly.portaMs = 300.0f;
    auto r6 = run (poly, { { 0.0, 33 }, { 0.4, -33 }, { 0.6, 45 }, { 1.4, -45 } }, 1.8, sine);
    const double fPolyStart = freqAt (r6.L, 0.62, 0.72);
    printf ("poly, no overlap: %.1f Hz right away (expect 110)\n", fPolyStart);
    ok &= std::abs (fPolyStart - 110) < 6;

    // poly with ALWAYS: glides even without overlap
    Params polyA = poly; polyA.portaAlways = true;
    auto r7 = run (polyA, { { 0.0, 33 }, { 0.4, -33 }, { 0.6, 45 }, { 1.4, -45 } }, 1.8, sine);
    const double fA = freqAt (r7.L, 0.62, 0.72);
    printf ("poly + ALWAYS: %.1f Hz at start (expect < 100)\n", fA);
    ok &= fA < 100;

    // granular modes on drums, key sync, transients
    Params g; g.grainOn = true; g.keySync = true; g.holdMs = 60;
    auto r8 = run (g, { { 0.0, 33 }, { 2.0, -33 } }, 2.5, drums);
    printf ("granular keysync: peak %.2f, left %d\n", r8.peak, r8.voicesLeft);
    ok &= r8.ok && r8.peak > 0.01 && r8.voicesLeft == 0;

    Params g2; g2.attack = 0; g2.holdMs = 4; g2.spacingMs = 3; g2.waveSpacing = -1.5f; g2.smear = 0.7f; g2.rand = 0.4f; g2.transients = true;
    auto r9 = run (g2, { { 0.0, 40 }, { 0.1, 47 }, { 2.0, -40 }, { 2.0, -47 } }, 2.5, drums);
    printf ("granular wild: peak %.2f, left %d\n", r9.peak, r9.voicesLeft);
    ok &= r9.ok && r9.peak < 6.0 && r9.voicesLeft == 0;

    printf (ok ? "ALL OK\n" : "FAILED\n");
    return ok ? 0 : 1;
}
