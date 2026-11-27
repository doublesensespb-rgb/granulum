#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
using namespace juce;
// MONO staccato melody with overlapping notes, SUSTAIN 0: peak of each note's first 40 ms
int main()
{
    ScopedJuceInitialiser_GUI init;
    const double sr = 44100.0; const int block = 64;
    GranulumProcessor p; p.setRateAndBufferSizeDetails (sr, block); p.prepareToPlay (sr, block); p.resetToFactoryInit();
    auto S = [&p] (const char* id, float v) { if (auto* x = p.apvts.getParameter (id)) x->setValueNotifyingHost (x->convertTo0to1 (v)); };
    S (ParamID::osc1On, 1); S (ParamID::osc1Mode, 1); S (ParamID::mono, 1); S (ParamID::porta, 0); S (ParamID::ampA, 0); S (ParamID::ampD, 300); S (ParamID::ampS, 0); S (ParamID::ampR, 300);
    if (auto* mm = MessageManager::getInstanceWithoutCreating()) mm->runDispatchLoopUntil (100);
    const int notes[] = { 40, 43, 47, 45, 40, 52, 50, 47 };
    const double len = 0.22, step = 0.18;   // each note overlaps the next by 40 ms
    AudioBuffer<float> buf (2, block); MidiBuffer midi;
    const int total = (int) (sr * (step * 8 + 0.5));
    std::vector<float> out ((size_t) total + block);
    for (int pos = 0; pos < total; pos += block)
    {
        midi.clear(); buf.clear();
        for (int k = 0; k < 8; ++k)
        {
            const int on = (int) (k * step * sr), off = (int) ((k * step + len) * sr);
            if (on >= pos && on < pos + block) midi.addEvent (MidiMessage::noteOn (1, notes[k], (uint8) 100), on - pos);
            if (off >= pos && off < pos + block) midi.addEvent (MidiMessage::noteOff (1, notes[k]), off - pos);
        }
        p.processBlock (buf, midi);
        for (int i = 0; i < block; ++i) out[(size_t) (pos + i)] = std::abs (buf.getSample (0, i));
    }
    for (int k = 0; k < 8; ++k)
    {
        const int on = (int) (k * step * sr); float pk = 0;
        for (int i = on; i < on + (int) (0.04 * sr); ++i) pk = std::max (pk, out[(size_t) i]);
        printf ("note %d: %6.1f dB\n", k + 1, Decibels::gainToDecibels (pk, -100.0f));
    }
}
