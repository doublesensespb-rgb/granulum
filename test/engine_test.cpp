// Offline tests for the v2 engine: pitch accuracy, grain transparency, formant keeps pitch,
// sampler auto-tune + 100 Hz high-pass, motion width, no NaN, fast onset.
#include "../Source/Engine.h"
#include <cstdio>
#include <cmath>
#include <string>

using namespace granulum;

struct Out { std::vector<float> sub, aL, aR, sL, sR; bool finite = true; int left = 0; };

static Out run (Params p, std::vector<std::pair<double, int>> ev, double seconds, const SampleData* smp, bool modulateAddPitch = false)
{
    Engine e; e.prepare (44100.0);
    Out o;
    const int block = 32, total = (int) (seconds * 44100);
    std::vector<float> s (block), al (block), ar (block), sl (block), sr (block);
    size_t k = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        while (k < ev.size() && ev[k].first * 44100 <= pos)
        {
            if (ev[k].second > 0) e.noteOn (ev[k].second, 1.0f, p, smp); else e.noteOff (-ev[k].second, p);
            ++k;
        }
        if (modulateAddPitch) p.add.pitch = (float) (12.0 * std::sin (pos / 44100.0 * 6.0));
        for (auto* b : { &s, &al, &ar, &sl, &sr }) std::fill (b->begin(), b->end(), 0.0f);
        e.render (s.data(), al.data(), ar.data(), sl.data(), sr.data(), block, p, smp);
        for (int i = 0; i < block; ++i)
        {
            for (float v : { s[(size_t) i], al[(size_t) i], ar[(size_t) i], sl[(size_t) i], sr[(size_t) i] })
                if (! std::isfinite (v)) o.finite = false;
            o.sub.push_back (s[(size_t) i]); o.aL.push_back (al[(size_t) i]); o.aR.push_back (ar[(size_t) i]);
            o.sL.push_back (sl[(size_t) i]); o.sR.push_back (sr[(size_t) i]);
        }
    }
    o.left = e.activeVoiceCount();
    return o;
}

static std::vector<float> slice (const std::vector<float>& x, double t0, double t1)
{
    return std::vector<float> (x.begin() + (long) (t0 * 44100), x.begin() + (long) (t1 * 44100));
}
static double rms (const std::vector<float>& x) { double s = 0; for (float v : x) s += v * v; return std::sqrt (s / std::max<size_t> (1, x.size())); }
static double pitchOf (const std::vector<float>& x, double t0, double t1) { return detectPitchHz (slice (x, t0, t1), 44100.0); }

int main()
{
    bool ok = true;
    auto check = [&ok] (bool c, const char* what) { printf ("%s %s\n", c ? "  ok " : "FAIL ", what); ok &= c; };

    Params base;
    base.harmonics[0] = 1.0f; base.harmonics[1] = 0.5f; base.harmonics[2] = 0.3f;
    base.sub.run = false; base.smp.run = false;

    // 1) ADD clean: A1 = 55 Hz
    Params clean = base; clean.add.grain = false;
    auto o1 = run (clean, { { 0, 33 }, { 1.0, -33 } }, 1.4, nullptr);
    const double f1 = pitchOf (o1.aL, 0.3, 0.8);
    printf ("clean ADD A1: %.2f Hz\n", f1);
    check (std::abs (f1 - 55.0) < 0.5 && o1.finite && o1.left == 0, "clean ADD pitch 55 Hz, voice ends");

    // 2) grains at TONE 0 are transparent (level + pitch)
    Params g0 = base; g0.texture = 0.0f; g0.tone = 0.0f; g0.amount = 1.0f;
    auto o2 = run (g0, { { 0, 33 }, { 1.0, -33 } }, 1.4, nullptr);
    const double r1 = rms (slice (o1.aL, 0.3, 0.8)), r2 = rms (slice (o2.aL, 0.3, 0.8));
    printf ("grain tone0: pitch %.2f Hz, level ratio %.2f\n", pitchOf (o2.aL, 0.3, 0.8), r2 / r1);
    check (std::abs (pitchOf (o2.aL, 0.3, 0.8) - 55.0) < 0.5 && r2 / r1 > 0.7 && r2 / r1 < 1.4, "grain at TONE 0 keeps pitch + level");

    // 3) TONE +12 changes timbre but keeps the 55 Hz pitch
    Params gt = g0; gt.tone = 12.0f; gt.texture = 0.3f;
    auto o3 = run (gt, { { 0, 33 }, { 1.0, -33 } }, 1.4, nullptr);
    const double f3 = pitchOf (o3.aL, 0.3, 0.8);
    printf ("TONE +12: pitch %.2f Hz\n", f3);
    check (std::abs (f3 - 55.0) < 1.0, "TONE moves formant, pitch stays");

    // 4) fast onset: sound within 10 ms of note-on even with TONE +12
    check (rms (slice (o3.aL, 0.0, 0.01)) > 0.01, "onset < 10 ms");

    // 5) MOTION creates stereo, no NaN
    Params gm = gt; gm.motion = 1.0f;
    auto o5 = run (gm, { { 0, 33 }, { 1.0, -33 } }, 1.4, nullptr);
    double diff = 0; for (size_t i = 13000; i < 35000; ++i) diff += std::abs (o5.aL[i] - o5.aR[i]);
    printf ("MOTION 1: L/R diff %.1f\n", diff);
    check (o5.finite && diff > 10.0, "MOTION adds width");

    // 6) smooth pitch modulation on ADD: no NaN, still sounding
    auto o6 = run (g0, { { 0, 33 }, { 1.5, -33 } }, 1.8, nullptr, true);
    check (o6.finite && rms (slice (o6.aL, 0.2, 1.4)) > 0.05, "smooth pitch modulation");

    // 7) sampler: 220 Hz sine sample -> detected root A3 (57), playing A2 (45) gives 110 Hz
    SampleData s; s.sampleRate = 48000.0;
    for (int i = 0; i < 48000 * 2; ++i) s.data.push_back ((float) (0.7 * std::sin (2 * kPi * 220.0 * i / 48000.0)));
    const double hz = detectPitchHz (s.data, s.sampleRate);
    s.detectedRoot = 69.0 + 12.0 * std::log2 (hz / 440.0);
    printf ("detected %.2f Hz -> root %.2f\n", hz, s.detectedRoot);
    check (std::abs (s.detectedRoot - 57.0) < 0.1, "auto-tune root detection");

    Params ps = base; ps.add.run = false; ps.smp.run = true; ps.smp.grain = false; ps.smpRoot = (float) s.detectedRoot;
    auto o7 = run (ps, { { 0, 45 }, { 1.0, -45 } }, 1.3, &s);
    const double f7 = pitchOf (o7.sL, 0.3, 0.8);
    printf ("sampler on A2: %.2f Hz\n", f7);
    check (std::abs (f7 - 110.0) < 1.0, "sampler plays in tune");

    // 8) sampler high-pass: 55 Hz note is strongly cut vs 220 Hz
    auto o8a = run (ps, { { 0, 33 }, { 1.0, -33 } }, 1.2, &s);
    auto o8b = run (ps, { { 0, 57 }, { 1.0, -57 } }, 1.2, &s);
    const double cutDb = 20.0 * std::log10 (rms (slice (o8a.sL, 0.4, 0.9)) / rms (slice (o8b.sL, 0.4, 0.9)));
    printf ("sampler 55 Hz vs 220 Hz: %.1f dB\n", cutDb);
    check (cutDb < -15.0, "sampler cut below 100 Hz");

    // 9) SUB: pure 55 Hz, mono
    Params pb = base; pb.add.run = false; pb.sub.run = true;
    auto o9 = run (pb, { { 0, 33 }, { 1.0, -33 } }, 1.2, nullptr);
    check (std::abs (pitchOf (o9.sub, 0.3, 0.8) - 55.0) < 0.5, "SUB 55 Hz");

    // 10) mono legato glide A1 -> A2
    Params gl = clean; gl.mono = true; gl.glideMs = 150.0f;
    auto o10 = run (gl, { { 0, 33 }, { 0.6, 45 }, { 1.5, -45 }, { 1.5, -33 } }, 1.9, nullptr);
    const double fa = pitchOf (o10.aL, 0.3, 0.55), fb = pitchOf (o10.aL, 0.9, 1.4);
    printf ("glide: %.1f -> %.1f Hz\n", fa, fb);
    check (std::abs (fa - 55) < 1 && std::abs (fb - 110) < 1.5, "legato glide");

    // 11) heavy settings + polyphony stress
    Params hv = g0; hv.texture = 1.0f; hv.tone = -12.0f; hv.motion = 0.8f; hv.mono = false; hv.smp.run = true;
    auto o11 = run (hv, { { 0, 24 }, { 0, 31 }, { 0, 36 }, { 0.05, 40 }, { 0.05, 43 }, { 0.1, 48 }, { 0.1, 52 }, { 0.1, 55 }, { 0.1, 60 },
                          { 1.0, -24 }, { 1.0, -31 }, { 1.0, -36 }, { 1.0, -40 }, { 1.0, -43 }, { 1.0, -48 }, { 1.0, -52 }, { 1.0, -55 }, { 1.0, -60 } }, 1.5, &s);
    double pk = 0; for (float v : o11.aL) pk = std::max (pk, (double) std::abs (v));
    printf ("stress: peak %.2f, left %d\n", pk, o11.left);
    check (o11.finite && pk < 20 && o11.left == 0, "stress test");

    printf (ok ? "ALL OK\n" : "FAILED\n");
    return ok ? 0 : 1;
}
