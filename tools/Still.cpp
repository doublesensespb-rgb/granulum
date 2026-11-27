#include <juce_audio_utils/juce_audio_utils.h>
#define private public
#include "PluginProcessor.h"
#undef private
using namespace juce;
// Still <preset> : silence 2 s, one note 0.5 s, then 3 s silence; prints UI values every 100 ms
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const double sr = 44100.0; const int block = 512;
    GranulumProcessor p; p.setRateAndBufferSizeDetails (sr, block); p.prepareToPlay (sr, block);
    if (String (argv[1]) != "init") p.loadPreset (File (argv[1]));
    else { for (auto* id : { "osc1Grain", "subOn", "osc3On", "osc3Grain" }) if (auto* q = p.apvts.getParameter (id)) q->setValueNotifyingHost (1.0f);
           p.setModulation (mtCutoff, 0, 0.5f); }
    if (auto* mm = MessageManager::getInstanceWithoutCreating()) mm->runDispatchLoopUntil (30);
    AudioBuffer<float> buf (2, block); MidiBuffer midi;
    const int total = (int) (sr * 6.0);
    int nextPrint = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        midi.clear(); buf.clear();
        const int on = (int) (sr * 2.0), off = (int) (sr * 2.5);
        if (on >= pos && on < pos + block) midi.addEvent (MidiMessage::noteOn (1, 52, (uint8) 100), on - pos);
        if (off >= pos && off < pos + block) midi.addEvent (MidiMessage::noteOff (1, 52), off - pos);
        p.processBlock (buf, midi);
        if (pos >= nextPrint)
        {
            nextPrint += (int) (sr * 0.25);
            printf ("t=%.2f sounding=%d cutoffNorm=%.3f eq3Gain=%.3f lfo1ph=%.3f lfo2ph=%.3f act[add %d clean %d wave %d smp %d noise %d sub %d] held=%d\n",
                    pos / sr, (int) p.uiSounding, p.modNorm[(size_t) mtCutoff].load(), p.modNorm[(size_t) mtEq3Gain].load(),
                    p.lfoPhaseOut[0].load(), p.lfoPhaseOut[1].load(),
                    (int) p.engineAdd.anyActive(), (int) p.engineAddClean.anyActive(), (int) p.engineWave.anyActive(),
                    (int) p.engineSmp.anyActive(), (int) p.engineNoise.anyActive(), (int) p.engineSub.anyActive(), p.heldNotes);
        }
    }
}
