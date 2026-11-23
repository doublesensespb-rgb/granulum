#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
using namespace juce;
// Loud <note> <outputDb> [stickMode] : render 2 s of INIT at note, print peak / RMS (dBFS)
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    if (String (argv[1]) == "sine")
    {
        // limiter alone on an 82 Hz sine, 2 s, at a given input peak (dB); write output for THD
        const float in = (float) atof (argv[2]); OutLimiter lim; lim.prepare (44100.0);
        const int n = 88200; std::vector<float> L (n), R (n);
        for (int i = 0; i < n; ++i) L[i] = R[i] = Decibels::decibelsToGain (in) * std::sin (2.0 * MathConstants<double>::pi * 82.0 * i / 44100.0);
        for (int i = 0; i < n; i += 512) lim.process (L.data() + i, R.data() + i, std::min (512, n - i), true, 0.9886f);
        FILE* f = fopen (argv[3], "wb"); fwrite (L.data(), 4, n, f); fclose (f);
        return 0;
    }
    const int note = atoi (argv[1]); const float outDb = (float) atof (argv[2]);
    const int mode = argc > 3 ? atoi (argv[3]) : 0;
    const double sr = 44100.0; const int block = 512;
    GranulumProcessor p; p.setRateAndBufferSizeDetails (sr, block); p.prepareToPlay (sr, block); p.resetToFactoryInit();
    auto setP = [&p] (const char* id, float v) { if (auto* x = p.apvts.getParameter (id)) x->setValueNotifyingHost (x->convertTo0to1 (v)); };
    setP (ParamID::output, outDb);
    if (mode == 1) setP (ParamID::clipOn, 0.0f);
    if (auto* mm = MessageManager::getInstanceWithoutCreating()) mm->runDispatchLoopUntil (200);
    const int total = (int) (sr * 2.0); AudioBuffer<float> buf (2, block); MidiBuffer midi;
    double sq = 0; float pk = 0; long n = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        midi.clear(); buf.clear();
        if (pos == 0) midi.addEvent (MidiMessage::noteOn (1, note, (uint8) 110), 0);
        p.processBlock (buf, midi);
        if (pos > sr * 0.5) for (int c = 0; c < 2; ++c) for (int i = 0; i < block; ++i) { const float x = buf.getSample (c, i); sq += x * x; pk = jmax (pk, std::abs (x)); ++n; }
    }
    printf ("note %d out %+5.1f clip %d  peak %6.2f dBFS  rms %6.2f dBFS\n", note, outDb, mode == 0, Decibels::gainToDecibels (pk), Decibels::gainToDecibels ((float) std::sqrt (sq / n)));
}
