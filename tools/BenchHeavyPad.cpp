#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace juce;
// The owner's heavy pad (Nov 23): POLY, D6/A6 + E6/B6 + F#6/C#7 pattern, size 600 ms, density 33/s, shimmer 5TH, AIR 34 %, pink noise grains, reverb
static float fromText (RangedAudioParameter* p, double target)
{
    float lo = 0, hi = 1;   // assumes the shown number grows with the value
    for (int i = 0; i < 40; ++i) { const float m = 0.5f * (lo + hi); if (p->getText (m, 32).getDoubleValue() < target) lo = m; else hi = m; }
    return 0.5f * (lo + hi);
}
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const double sr = 44100.0; const int block = 256; const double bpm = 140.0;
    const float relMs = argc > 1 ? (float) atof (argv[1]) : 2000.0f;
    GranulumProcessor p; p.setRateAndBufferSizeDetails (sr, block); p.prepareToPlay (sr, block); p.resetToFactoryInit();
    auto S = [&p] (const char* id, float v) { if (auto* x = p.apvts.getParameter (id)) x->setValueNotifyingHost (x->convertTo0to1 (v)); };
    auto T = [&p] (const char* id, double v) { if (auto* x = p.apvts.getParameter (id)) x->setValueNotifyingHost (fromText (x, v)); };
    S (ParamID::osc1On, 1); S (ParamID::osc1Grain, 1); S (ParamID::osc1Vol, 0.74f); S (ParamID::osc1Air, 0.34f);
    S (ParamID::osc3On, 1); S (ParamID::osc3Grain, 1);
    S (ParamID::bloom, .51f); S (ParamID::smear, .40f); S (ParamID::scatter, .49f); S (ParamID::shimmer, .42f); S (ParamID::drift, .61f); S (ParamID::chime, .01f);
    T (ParamID::gsize, 600); S (ParamID::speed, 0.18f); S (ParamID::position, 0.03f); T (ParamID::density, 33); S (ParamID::spread, .62f); S (ParamID::gmix, .87f);
    S (ParamID::shimmerMode, 1); S (ParamID::filterOn, 1); S (ParamID::revOn, 1); S (ParamID::revMix, .51f); S (ParamID::mono, 0);
    S (ParamID::ampR, relMs); S (ParamID::ampA, 300.0f); S (ParamID::ampS, 0.8f);
    const String off = argc > 2 ? String (argv[2]) : String();
    if (off.contains ("air")) S (ParamID::osc1Air, 0);
    if (off.contains ("noise")) S (ParamID::osc3On, 0);
    if (off.contains ("shim")) { S (ParamID::shimmer, 0); }
    if (off.contains ("rev")) S (ParamID::revOn, 0);
    if (off.contains ("filt")) S (ParamID::filterOn, 0);
    if (off.contains ("small")) T (ParamID::gsize, 150);
    if (off.contains ("bloom")) S (ParamID::bloom, 0);
    if (off.contains ("drift")) S (ParamID::drift, 0);
    if (off.contains ("smear")) S (ParamID::smear, 0);
    if (const char* vv = std::getenv ("VOICES")) S (ParamID::osc1Voices, (float) atoi (vv));
    if (std::getenv ("CLEAN")) { S (ParamID::osc1Grain, 0); S (ParamID::osc3On, 0); }
    if (std::getenv ("WAVE")) S (ParamID::osc1Mode, 1);
    if (auto* mm = MessageManager::getInstanceWithoutCreating()) mm->runDispatchLoopUntil (200);
    std::cout << "size " << p.apvts.getParameter (ParamID::gsize)->getCurrentValueAsText() << "  dens " << p.apvts.getParameter (ParamID::density)->getCurrentValueAsText() << "  rel " << relMs << std::endl;
    // pattern in beats: (start, len, note)
    struct N { double s, l; int n; };
    const int D6 = 86, A6 = 93, E6 = 88, B6 = 95, Fs6 = 90, Cs7 = 97;
    std::vector<N> pat { {0,4.5,D6},{0,4.5,A6},{4,2.5,E6},{4,2.5,B6},{6,2.5,Fs6},{6,2.5,Cs7},{8,4.5,D6},{8,4.5,A6},{12,2.5,E6},{12,2.5,B6},{14,2.5,Fs6},{14,2.5,Cs7} };
    const double spb = sr * 60.0 / bpm; const int total = (int) (spb * (argc > 3 ? atof (argv[3]) : 32.0));
    AudioBuffer<float> buf (2, block); MidiBuffer midi;
    double sum = 0, worst = 0, worstAt = 0, sq = 0; long nsq = 0; FILE* wav = argc > 4 ? fopen (argv[4], "wb") : nullptr; int nb = 0; int over = 0;
    const double budget = 1000.0 * block / sr;
    for (int pos = 0; pos < total; pos += block)
    {
        midi.clear(); buf.clear();
        for (int rep = 0; rep < 2; ++rep)
            for (auto& x : pat)
            {
                const int on = (int) ((x.s + rep * 16) * spb), off = (int) ((x.s + x.l + rep * 16) * spb);
                if (on >= pos && on < pos + block) midi.addEvent (MidiMessage::noteOn (1, x.n, (uint8) 100), on - pos);
                if (off >= pos && off < pos + block) midi.addEvent (MidiMessage::noteOff (1, x.n), off - pos);
            }
        const auto t0 = std::chrono::steady_clock::now();
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < block; ++i) { const float x = buf.getSample (c, i); sq += x * x; } nsq += 2 * block;
        if (wav) { for (int i = 0; i < block; ++i) { float fr[2] = { buf.getSample (0, i), buf.getSample (1, i) }; fwrite (fr, 4, 2, wav); } }
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        sum += ms; if (ms > worst) { worst = ms; worstAt = pos / spb; } ++nb; if (ms > budget * 0.5) ++over;
    }
    printf ("rms %.2f dB  ", 10 * std::log10 (sq / nsq + 1e-20)); if (wav) fclose (wav); printf ("worst at beat %.2f  ", worstAt); printf ("avg %.1f %% of one core, worst block %.2f ms of %.2f ms (%.0f %%), blocks over 50%%: %d / %d\n", 100.0 * sum / (nb * budget), worst, budget, 100 * worst / budget, over, nb);
}
