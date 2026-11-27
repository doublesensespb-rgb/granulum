#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
using namespace juce;
// Sub <preset> <note> <secs> <out.raw> : one held note, writes L channel
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const double sr = 44100.0; const int block = 256;
    GranulumProcessor p; p.setRateAndBufferSizeDetails (sr, block); p.prepareToPlay (sr, block);
    p.loadPreset (File (argv[1]));
    if (auto* mm = MessageManager::getInstanceWithoutCreating()) mm->runDispatchLoopUntil (50);
    const int note = atoi (argv[2]); const int total = (int) (sr * atof (argv[3]));
    AudioBuffer<float> buf (2, block); MidiBuffer midi; FILE* f = fopen (argv[4], "wb");
    for (int pos = 0; pos < total; pos += block)
    {
        midi.clear(); buf.clear();
        if (pos == 0) midi.addEvent (MidiMessage::noteOn (1, note, (uint8) 100), 0);
        p.processBlock (buf, midi);
        fwrite (buf.getReadPointer (0), 4, (size_t) block, f);
    }
    fclose (f);
}
