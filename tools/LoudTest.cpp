#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
using namespace juce;
// Loud <note> <outputDb> [stickMode] : render 2 s of INIT at note, print peak / RMS (dBFS)
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const int note = atoi (argv[1]); const float outDb = (float) atof (argv[2]);
    const int mode = argc > 3 ? atoi (argv[3]) : 0;
    const double sr = 44100.0; const int block = 512;
    GranulumProcessor p; p.setRateAndBufferSizeDetails (sr, block); p.prepareToPlay (sr, block); p.resetToFactoryInit();
    auto setP = [&p] (const char* id, float v) { if (auto* x = p.apvts.getParameter (id)) x->setValueNotifyingHost (x->convertTo0to1 (v)); };
    setP (ParamID::output, outDb);
    if (mode == 1) setP (ParamID::clipOn, 0.0f);
    FILE* wf = argc > 4 ? fopen (argv[4], "wb") : nullptr;
    if (auto* mm = MessageManager::getInstanceWithoutCreating()) mm->runDispatchLoopUntil (200);
    const int total = (int) (sr * 2.0); AudioBuffer<float> buf (2, block); MidiBuffer midi;
    double sq = 0; float pk = 0; long n = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        midi.clear(); buf.clear();
        if (pos == 0) midi.addEvent (MidiMessage::noteOn (1, note, (uint8) 110), 0);
        p.processBlock (buf, midi);
        if (wf) for (int i = 0; i < block; ++i) { const float x = buf.getSample (0, i); fwrite (&x, 4, 1, wf); }
        if (pos > sr * 0.5) for (int c = 0; c < 2; ++c) for (int i = 0; i < block; ++i) { const float x = buf.getSample (c, i); sq += x * x; pk = jmax (pk, std::abs (x)); ++n; }
    }
    if (wf) fclose (wf);
    printf ("note %d out %+5.1f clip %d  peak %6.2f dBFS  rms %6.2f dBFS\n", note, outDb, mode == 0, Decibels::gainToDecibels (pk), Decibels::gainToDecibels ((float) std::sqrt (sq / n)));
}
