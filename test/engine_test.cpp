// Offline tests: pitch, chords, macros don't explode, voices end, evolve ramps in.
#include "../Source/GrainEngine.h"
#include <cstdio>
#include <cmath>

using namespace granulum;

struct R { std::vector<float> L, Rr; bool finite = true; int left = 0; };

static R run (Params p, std::vector<std::pair<double, int>> ev, double seconds, const SampleData& s)
{
    GrainEngine e; e.prepare (44100.0);
    R r;
    const int block = 64, total = (int) (seconds * 44100);
    std::vector<float> L (block), Rb (block);
    size_t k = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        while (k < ev.size() && ev[k].first * 44100 <= pos)
        {
            if (ev[k].second > 0) e.noteOn (ev[k].second, 1.0f, p); else e.noteOff (-ev[k].second, p);
            ++k;
        }
        std::fill (L.begin(), L.end(), 0.0f); std::fill (Rb.begin(), Rb.end(), 0.0f);
        e.render (L.data(), Rb.data(), block, p, &s);
        for (int i = 0; i < block; ++i)
        {
            if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (Rb[(size_t) i])) r.finite = false;
            r.L.push_back (L[(size_t) i]); r.Rr.push_back (Rb[(size_t) i]);
        }
    }
    r.left = e.activeVoiceCount();
    return r;
}

static double rms (const std::vector<float>& x, double t0, double t1)
{
    double s = 0; int n = 0;
    for (int i = (int) (t0 * 44100); i < (int) (t1 * 44100) && i < (int) x.size(); ++i) { s += x[(size_t) i] * x[(size_t) i]; ++n; }
    return std::sqrt (s / std::max (1, n));
}
static double freqAt (const std::vector<float>& x, double t0, double t1)
{
    int c = 0;
    for (int i = (int) (t0 * 44100) + 1; i < (int) (t1 * 44100); ++i) if (x[(size_t) i - 1] <= 0 && x[(size_t) i] > 0) ++c;
    return c / (t1 - t0);
}
static double dftMag (const std::vector<float>& x, double f, double t0, double t1)
{
    double re = 0, im = 0;
    for (int i = (int) (t0 * 44100); i < (int) (t1 * 44100); ++i) { const double ph = 2 * kPi * f * i / 44100.0; re += x[(size_t) i] * std::cos (ph); im += x[(size_t) i] * std::sin (ph); }
    return std::sqrt (re * re + im * im);
}

int main()
{
    bool ok = true;
    auto check = [&ok] (bool c, const char* w) { printf ("%s %s\n", c ? "  ok " : "FAIL ", w); ok &= c; };

    std::array<float, kNumHarmonics> h {}; h[0] = 1.0f;
    const auto sine = renderAdditive (h);

    Params clean; clean.grainOn = false;
    auto a = run (clean, { { 0, 33 }, { 1.0, -33 } }, 1.5, sine);
    printf ("clean A1 %.1f Hz\n", freqAt (a.L, 0.2, 0.9));
    check (std::abs (freqAt (a.L, 0.2, 0.9) - 55) < 1.5 && a.left == 0, "clean pitch + release");

    // chord MAJ on A2: energy at 110, 138.6 (C#3), 164.8 (E3)
    Params ch = clean; ch.chord = { 0, 4, 7, 0, 0 }; ch.chordSize = 3; ch.mono = false;
    auto c = run (ch, { { 0, 45 }, { 1.0, -45 } }, 1.5, sine);
    const double m1 = dftMag (c.L, 110.0, 0.2, 0.9), m3 = dftMag (c.L, 138.59, 0.2, 0.9), m5 = dftMag (c.L, 164.81, 0.2, 0.9), off = dftMag (c.L, 123.47, 0.2, 0.9);
    printf ("chord bins: root %.0f third %.0f fifth %.0f off %.0f\n", m1, m3, m5, off);
    check (m3 > off * 10 && m5 > off * 10 && m1 > off * 10 && c.left == 0, "major chord in poly");

    Params chm = ch; chm.mono = true;
    auto cm = run (chm, { { 0, 45 }, { 0.5, 50 }, { 1.0, -50 }, { 1.0, -45 } }, 1.5, sine);
    check (dftMag (cm.L, 146.83, 0.6, 0.95) > dftMag (cm.L, 110.0, 0.6, 0.95) * 3 && cm.left == 0, "chord in mono follows legato note");

    // grains with every macro high: finite, sane level, stereo, ends
    Params g; g.grainOn = true; g.bloom = 1; g.smear = 1; g.scatter = 1; g.shimmer = 1; g.drift = 1; g.density = 1; g.size = 1; g.speed = -1.5f; g.mix = 1;
    auto gm = run (g, { { 0, 40 }, { 0, 47 }, { 2.0, -40 }, { 2.0, -47 } }, 2.6, sine);
    double pk = 0, diff = 0;
    for (size_t i = 0; i < gm.L.size(); ++i) { pk = std::max (pk, (double) std::abs (gm.L[i])); diff += std::abs (gm.L[i] - gm.Rr[i]); }
    printf ("all macros max: peak %.2f, L/R diff %.0f, left %d\n", pk, diff, gm.left);
    check (gm.finite && pk < 4.0 && diff > 100 && gm.left == 0, "macros stable + stereo");

    // grains at defaults keep roughly the clean level
    Params gd; gd.grainOn = true;
    auto d = run (gd, { { 0, 45 }, { 1.0, -45 } }, 1.4, sine);
    const double ratio = rms (d.L, 0.3, 0.9) / rms (a.L, 0.3, 0.9);
    printf ("grain default level vs clean: %.2f\n", ratio);
    check (ratio > 0.4 && ratio < 2.5, "grain default level");

    // EVOLVE: starts clean, ends grainy
    Params evp; evp.grainOn = true; evp.evolve = 2.0f; evp.bloom = 1; evp.scatter = 1;
    auto ev = run (evp, { { 0, 45 }, { 3.0, -45 } }, 3.4, sine);
    double dStart = 0, dEnd = 0;
    for (int i = (int) (0.05 * 44100); i < (int) (0.3 * 44100); ++i) dStart += std::abs (ev.L[(size_t) i] - ev.Rr[(size_t) i]);
    for (int i = (int) (2.4 * 44100); i < (int) (2.65 * 44100); ++i) dEnd += std::abs (ev.L[(size_t) i] - ev.Rr[(size_t) i]);
    printf ("evolve width start %.1f end %.1f\n", dStart, dEnd);
    check (dEnd > dStart * 5, "EVOLVE blooms over time");

    printf (ok ? "ALL OK\n" : "FAILED\n");
    return ok ? 0 : 1;
}
