#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
using namespace juce;
// Filt: white noise through every FilterUnit type; level (dB vs. the dry noise) at fc/4, fc/2, fc, 2fc, 4fc for several RESO
static double bandDb (const std::vector<float>& x, const std::vector<float>& d, double f, double sr)
{
    // energy ratio in a 1/3-octave band via Goertzel-like DFT sums over many bins
    const int N = (int) x.size(); double ex = 0, ed = 0;
    const int k0 = (int) (f * std::pow (2.0, -1.0 / 6.0) * N / sr), k1 = (int) (f * std::pow (2.0, 1.0 / 6.0) * N / sr);
    for (int k = k0; k <= k1; k += 1)
    {
        double rx = 0, ix = 0, rd = 0, id = 0;
        const double w = 2 * MathConstants<double>::pi * k / N;
        for (int i = 0; i < N; i += 1) { const double c = std::cos (w * i), s = std::sin (w * i); rx += x[i] * c; ix += x[i] * s; rd += d[i] * c; id += d[i] * s; }
        ex += rx * rx + ix * ix; ed += rd * rd + id * id;
    }
    return 10 * std::log10 (ex / ed + 1e-20);
}
int main()
{
    const double sr = 44100.0; const int N = 8192;
    Random r (7);
    std::vector<float> dry (N); for (auto& v : dry) v = r.nextFloat() * 2 - 1;
    const char* names[] = { "LP12", "LP24", "LP48", "HP12", "HP24", "HP48", "BP" };
    for (float fc : { 300.0f, 2000.0f })
        for (int t = 0; t <= 6; ++t)
        {
            printf ("%-5s fc %5.0f |", names[t], fc);
            for (float q : { 0.707f, 2.0f, 5.0f, 10.0f })
            {
                FilterUnit f; f.prepare (sr); f.reset(); f.classic = std::getenv ("CLASSIC") != nullptr;
                std::vector<float> L (dry), R (dry);
                // run twice the length to settle, measure the second half
                std::vector<float> L2 (N * 2), R2 (N * 2);
                for (int i = 0; i < N * 2; ++i) L2[(size_t) i] = R2[(size_t) i] = dry[(size_t) (i % N)];
                for (int pos = 0; pos < N * 2; pos += 32) f.process (L2.data() + pos, R2.data() + pos, 32, t, fc, q, 0.0f, 1.0f);
                std::vector<float> out (L2.begin() + N, L2.end());
                printf (" q%-4.1f", q);
                for (double m : { 0.25, 0.5, 1.0, 2.0, 4.0 }) printf (" %6.1f", bandDb (out, dry, fc * m, sr));
                printf (" |");
            }
            printf ("\n");
        }
}
