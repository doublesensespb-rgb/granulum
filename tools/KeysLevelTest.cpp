#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
using namespace juce;
// Keys <type 0..5> <seedRuns> : one random preset of that type, played at several notes; prints RMS / peak per note
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const int type = atoi (argv[1]); const int runs = argc > 2 ? atoi (argv[2]) : 1;
    const double sr = 44100.0; const int block = 512;
    for (int r = 0; r < runs; ++r)
    {
        Random::getSystemRandom().setSeed (1234 + r); GranulumProcessor p; p.setRateAndBufferSizeDetails (sr, block); p.prepareToPlay (sr, block); p.resetToFactoryInit();
        static MemoryBlock saved[64];
        if (std::getenv ("KEEP") == nullptr) { if (type > 0) p.randomizeTyped (type); MemoryBlock mb; p.getStateInformation (mb); File ("/tmp/keys_" + String (type) + "_" + String (r) + ".bin").replaceWithData (mb.getData(), mb.getSize()); }
        else { MemoryBlock mb; File ("/tmp/keys_" + String (type) + "_" + String (r) + ".bin").loadFileAsData (mb); p.setStateInformation (mb.getData(), (int) mb.getSize()); }
        const String off = argc > 3 ? String (argv[3]) : String();
        auto S = [&p] (const char* id, float v) { if (auto* x = p.apvts.getParameter (id)) x->setValueNotifyingHost (x->convertTo0to1 (v)); };
        if (off.contains ("filt")) { S (ParamID::filterOn, 0); S (ParamID::filter2On, 0); }
        if (off.contains ("grain")) { S (ParamID::osc1Grain, 0); S (ParamID::osc2Grain, 0); S (ParamID::osc3Grain, 0); }
        for (auto id : { "chime", "drift", "shimmer", "smear", "bloom", "scatter", "osc1Air", "spread" })
            if (off.contains (String ("z_") + id)) S (id, 0);
        if (off.contains ("fx")) { S (ParamID::revOn, 0); S (ParamID::delayOn, 0); S (ParamID::distOn, 0); }
        if (auto* mm = MessageManager::getInstanceWithoutCreating()) mm->runDispatchLoopUntil (100);
        printf ("%s #%d:", GranulumProcessor::randTypeNames()[type].toRawUTF8(), r);
        for (int note : { 28, 40, 52, 64, 76, 88 })
        {
            AudioBuffer<float> buf (2, block); MidiBuffer midi; double sq = 0; long n = 0;
            for (int pos = 0; pos < (int) (sr * 1.6); pos += block)
            {
                midi.clear(); buf.clear();
                if (pos == 0) midi.addEvent (MidiMessage::noteOn (1, note, (uint8) 100), 0);
                if (pos + block >= (int) (sr * 1.2) && pos < (int) (sr * 1.2)) midi.addEvent (MidiMessage::allNotesOff (1), 0);
                p.processBlock (buf, midi);
                if (pos > sr * 0.15 && pos < sr * 1.1) for (int c = 0; c < 2; ++c) for (int i = 0; i < block; ++i) { const float x = buf.getSample (c, i); sq += x * x; ++n; }
            }
            for (int k = 0; k < 200; ++k) { buf.clear(); midi.clear(); p.processBlock (buf, midi); }   // let tails die
            printf ("  %s %6.1f", MidiMessage::getMidiNoteName (note, true, true, 3).toRawUTF8(), 10 * std::log10 (sq / std::max (1L, n) + 1e-12));
        }
        printf ("\n");
    }
}
