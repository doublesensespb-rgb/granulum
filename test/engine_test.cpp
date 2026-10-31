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

    // CHIME: grains of plain noise ring at the note's harmonics (A3 = 220 Hz)
    SampleData noise; noise.sampleRate = 44100.0; noise.channels.assign (1, std::vector<float> (44100 * 2));
    { uint32_t sd = 1; for (auto& x : noise.channels[0]) { sd = sd * 1664525u + 1013904223u; x = ((float) (sd >> 8) / 16777216.0f * 2.0f - 1.0f) * 0.3f; } }
    Params cp; cp.grainOn = true; cp.keyTrack = false; cp.randomStart = true;
    auto c0 = run (cp, { { 0, 57 }, { 1.2, -57 } }, 1.2, noise);
    cp.keyTrack = true; cp.chime = 0.45f;
    auto c1 = run (cp, { { 0, 57 }, { 1.2, -57 } }, 1.2, noise);
    // A3 (220 Hz) is lifted to 880 Hz: rings 880, 1760 ... but adds nothing in the low mids
    const double h1 = dftMag (c1.L, 880.0, 0.3, 1.1), h2 = dftMag (c1.L, 1760.0, 0.3, 1.1), between = dftMag (c1.L, 1320.0, 0.3, 1.1);
    const double low0 = dftMag (c0.L, 220.0, 0.3, 1.1) + dftMag (c0.L, 330.0, 0.3, 1.1), low1 = dftMag (c1.L, 220.0, 0.3, 1.1) + dftMag (c1.L, 330.0, 0.3, 1.1);
    printf ("chime 0.45: 880 %.0f  1760 %.0f  1320 %.0f | low mids %.0f -> %.0f\n", h1, h2, between, low0, low1);
    check (h1 > between * 4 && h2 > between * 2, "CHIME rings in key, up in the air");
    check (low1 < low0 * 1.6, "CHIME leaves the low mids alone");
    double sd = 0; for (size_t i = (size_t) (0.3 * 44100); i < (size_t) (1.1 * 44100); ++i) sd += std::abs (c1.L[i] - c1.Rr[i]);
    cp.chime = 1.0f;
    auto c2 = run (cp, { { 0, 57 }, { 1.2, -57 } }, 3.0, noise);
    const double bell = dftMag (c2.L, 880.0 * 2.756, 0.3, 1.1), harm3 = dftMag (c2.L, 2640.0, 0.3, 1.1);
    const double lv = rms (c2.L, 0.3, 1.1) / rms (c0.L, 0.3, 1.1);
    printf ("chime 1.0: bell partial %.0f vs 3rd harmonic %.0f, level x%.2f, tail %.4f, left %d, stereo %.0f\n", bell, harm3, lv, rms (c2.L, 1.3, 1.6), c2.left, sd);
    check (c2.finite && bell > harm3 * 2 && lv > 0.5 && lv < 4.0 && rms (c2.L, 1.3, 1.6) > 1e-4 && c2.left == 0, "CHIME at max: bell, sane level, rings out, ends");

    // per-harmonic split: grain part + clean part == the full sound (grains off)
    std::array<float, kNumHarmonics> hs {}; hs[0] = 1; hs[1] = 0.5f; hs[2] = 0.7f; hs[5] = 0.3f;
    auto full = renderAdditive (hs);
    auto parts = renderAdditiveSplit (hs, 0xfffcu);   // harmonics 1-2 clean
    double err = 0, e1 = 0;
    for (size_t i = 0; i < full.channels[0].size(); ++i)
    {
        err += std::abs (full.channels[0][i] - parts.first.channels[0][i] - parts.second.channels[0][i]);
        e1 += std::abs (parts.second.channels[0][i]);
    }
    printf ("split error %.6f, clean energy %.1f\n", err, e1);
    check (err < 1e-2 && e1 > 100, "per-harmonic split adds up to the full sound");

    // keyTrack off: low and high notes play the source at the same pitch
    Params kt = clean; kt.keyTrack = false;
    auto k1 = run (kt, { { 0, 40 }, { 0.8, -40 } }, 1.0, sine);
    auto k2 = run (kt, { { 0, 70 }, { 0.8, -70 } }, 1.0, sine);
    check (std::abs (freqAt (k1.L, 0.2, 0.7) - freqAt (k2.L, 0.2, 0.7)) < 1.0, "keyTrack off ignores the note");

    printf (ok ? "ALL OK\n" : "FAILED\n");
    return ok ? 0 : 1;
}
