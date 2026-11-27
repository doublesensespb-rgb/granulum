#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
using namespace juce;
// Switch <presetA> <presetB> : play A (note E4, 1 s), load B like the browser does, play E4 again; prints B's level per 25 ms
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const double sr = 44100.0; const int block = 256;
    GranulumProcessor p; p.setRateAndBufferSizeDetails (sr, block); p.prepareToPlay (sr, block);
    AudioBuffer<float> buf (2, block); MidiBuffer midi;
    auto play = [&] (double secs, bool print)
    {
        const int total = (int) (sr * secs); const int win = (int) (sr * 0.025); double sq = 0; float pk = 0; int cnt = 0, w = 0;
        String line;
        for (int pos = 0; pos < total; pos += block)
        {
            midi.clear(); buf.clear();
            if (pos == 0) midi.addEvent (MidiMessage::noteOn (1, 64, (uint8) 100), 0);
            if (pos + block >= total) midi.addEvent (MidiMessage::noteOff (1, 64), 0);
            p.processBlock (buf, midi);
            for (int i = 0; i < block; ++i) { const float x = std::max (std::abs (buf.getSample (0, i)), std::abs (buf.getSample (1, i))); sq += x * x; pk = std::max (pk, x); if (++cnt == win) { if (print && w < 24) line << String (Decibels::gainToDecibels (pk), 0) << " "; ++w; cnt = 0; sq = 0; pk = 0; } }
        }
        if (print) printf ("%s\n", line.toRawUTF8());
    };
    p.loadPreset (File (argv[1]));
    if (auto* mm = MessageManager::getInstanceWithoutCreating()) mm->runDispatchLoopUntil (50);
    play (1.0, false);
    for (int k = 0; k < 40; ++k) { buf.clear(); midi.clear(); p.processBlock (buf, midi); }
    p.loadPreset (File (argv[2]));
    if (argc > 3) for (auto id : StringArray::fromTokens (argv[3], ",", "")) if (auto* x = p.apvts.getParameter (id)) x->setValueNotifyingHost (0.0f);
    if (auto* mm = MessageManager::getInstanceWithoutCreating()) mm->runDispatchLoopUntil (50);
    if (std::getenv ("DUMP")) { auto st = p.apvts.copyState(); if (auto x = st.createXml()) x->writeTo (File (std::getenv ("DUMP"))); }
    printf ("first note after switch (peak dB per 25 ms):\n"); play (1.0, true);
    for (int k = 0; k < 40; ++k) { buf.clear(); midi.clear(); p.processBlock (buf, midi); }
    printf ("second note, same preset:\n"); play (1.0, true);
}
