#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
using namespace juce;
// PRen <outdir> <preset...> : each preset, 4 notes MIDI 52 + 47 (1/4 at 120 BPM, gate 80 %), raw float stereo
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const double sr = 44100.0; const int block = 128; File out (argv[1]); out.createDirectory();
    for (int a = 2; a < argc; ++a)
    {
        GranulumProcessor p; p.setRateAndBufferSizeDetails (sr, block); p.prepareToPlay (sr, block);
        p.loadPreset (File (argv[a]));
        if (auto* mm = MessageManager::getInstanceWithoutCreating()) mm->runDispatchLoopUntil (30);
        const int beat = (int) (sr * 0.5), total = beat * 5;
        AudioBuffer<float> buf (2, block); MidiBuffer midi;
        FILE* f = fopen (out.getChildFile (File (argv[a]).getFileNameWithoutExtension() + ".raw").getFullPathName().toRawUTF8(), "wb");
        for (int pos = 0; pos < total; pos += block)
        {
            midi.clear(); buf.clear();
            for (int n = 0; n < 4; ++n)
            {
                const int note = n % 2 ? 47 : 52, on = n * beat, off = on + (int) (beat * 0.8);
                if (off >= pos && off < pos + block) midi.addEvent (MidiMessage::noteOff (1, note), off - pos);
                if (on >= pos && on < pos + block) midi.addEvent (MidiMessage::noteOn (1, note, (uint8) 100), on - pos);
            }
            p.processBlock (buf, midi);
            for (int i = 0; i < block; ++i) for (int c = 0; c < 2; ++c) { float s = buf.getSample (c, i); fwrite (&s, 4, 1, f); }
        }
        fclose (f);
    }
}
