#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
using namespace juce;
// Notes <preset> <out.raw> <note...> : each note held 0.6 s + 0.4 s gap, mono raw float; SET=id=v,... overrides
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const double sr = 44100.0; const int block = 128;
    GranulumProcessor p; p.setRateAndBufferSizeDetails (sr, block); p.prepareToPlay (sr, block);
    p.loadPreset (File (argv[1]));
    if (auto* e = std::getenv ("SET")) { StringArray kv; kv.addTokens (e, ",", ""); for (auto& t : kv) {
        auto id = t.upToFirstOccurrenceOf ("=", false, false); float v = t.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
        if (auto* x = p.apvts.getParameter (id)) x->setValueNotifyingHost (x->convertTo0to1 (v)); } }
    if (auto* mm = MessageManager::getInstanceWithoutCreating()) mm->runDispatchLoopUntil (30);
    FILE* f = fopen (argv[2], "wb");
    AudioBuffer<float> buf (2, block); MidiBuffer midi;
    for (int a = 3; a < argc; ++a)
    {
        const int note = atoi (argv[a]), on = 0, off = (int) (sr * 0.6), total = (int) (sr * 1.0);
        for (int pos = 0; pos < total; pos += block)
        {
            midi.clear(); buf.clear();
            if (on >= pos && on < pos + block) midi.addEvent (MidiMessage::noteOn (1, note, (uint8) 100), on - pos);
            if (off >= pos && off < pos + block) midi.addEvent (MidiMessage::noteOff (1, note), off - pos);
            p.processBlock (buf, midi);
            for (int i = 0; i < block; ++i) { float s[2] = { buf.getSample (0, i), buf.getSample (1, i) }; fwrite (s, 4, 2, f); }
        }
    }
    fclose (f);
}
