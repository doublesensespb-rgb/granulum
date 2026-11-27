#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
using namespace juce;
// Bars2 <preset> <out.raw> : irregular 1-bar mono pattern repeated 8 bars at 140 BPM with a playing host
struct PH : AudioPlayHead { double ppq = 0; Optional<PositionInfo> getPosition() const override {
    PositionInfo i; i.setBpm (140.0); i.setPpqPosition (ppq); i.setIsPlaying (true); return i; } };
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const double sr = 44100.0; const int block = 128;
    GranulumProcessor p; p.setRateAndBufferSizeDetails (sr, block); p.prepareToPlay (sr, block);
    PH ph; p.setPlayHead (&ph);
    p.loadPreset (File (argv[1]));
    if (auto* e = std::getenv ("SET")) { StringArray kv; kv.addTokens (e, ",", ""); for (auto& t : kv) {
        auto id = t.upToFirstOccurrenceOf ("=", false, false); float v = t.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
        if (auto* x = p.apvts.getParameter (id)) x->setValueNotifyingHost (x->convertTo0to1 (v)); else printf ("no %s\n", id.toRawUTF8()); } }
    if (auto* mm = MessageManager::getInstanceWithoutCreating()) mm->runDispatchLoopUntil (50);
    const double s16 = sr * 60.0 / 140.0 / 4.0; const int bar = (int) std::lround (s16 * 16);
    // {start16, len16, note}  (overlap = legato in mono)
    const int pat[][3] = { {0,2,28},{2,1,28},{3,3,31},{6,1,40},{7,2,28},{9,1,35},{10,3,28},{12,1,33},{13,3,28} };
    const int total = bar * 8;
    AudioBuffer<float> buf (2, block); MidiBuffer midi; FILE* f = fopen (argv[2], "wb");
    for (int pos = 0; pos < total; pos += block)
    {
        midi.clear(); buf.clear(); ph.ppq = pos / (sr * 60.0 / 140.0);
        for (int b = 0; b < 8; ++b) for (auto& n : pat)
        {
            const int on = b * bar + (int) (n[0] * s16), off = b * bar + (int) ((n[0] + n[1]) * s16 * 0.95);
            if (off >= pos && off < pos + block) midi.addEvent (MidiMessage::noteOff (1, n[2]), off - pos);
            if (on >= pos && on < pos + block) midi.addEvent (MidiMessage::noteOn (1, n[2], (uint8) 100), on - pos);
        }
        p.processBlock (buf, midi);
        for (int i = 0; i < block; ++i) { const float m = 0.5f * (buf.getSample (0, i) + buf.getSample (1, i)); fwrite (&m, 4, 1, f); }
    }
    fclose (f); printf ("bar %d\n", bar);
}
