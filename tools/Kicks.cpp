#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
using namespace juce;
// Kicks <outdir> : builds the soft deep kick presets (folder <outdir>/Kicks) and one WAV with all of them (4 hits each, FL E4)
struct K { const char* name; int semi; float pDepth, pRate, pCurve, decay, rel, osc1, sub, h2, drive, cutoff, click; };
static const K kicks[] = {
    { "Kick_01", 5, 0.50f, 18.0f, -0.80f, 180.0f,  60.0f, 0.80f, 0.45f, 0.00f, 0.15f, 2500.0f, 0.55f },  // short, tight
    { "Kick_02", 3, 0.45f, 12.0f, -0.80f, 260.0f,  80.0f, 0.80f, 0.50f, 0.00f, 0.20f, 1800.0f, 0.45f },  // round
    { "Kick_03", 1, 0.45f,  9.0f, -0.75f, 450.0f, 120.0f, 0.75f, 0.55f, 0.00f, 0.25f, 1500.0f, 0.40f },  // warm, long
    { "Kick_04", 4, 0.40f, 14.0f, -0.75f, 220.0f,  70.0f, 0.80f, 0.45f, 0.00f, 0.10f, 1200.0f, 0.35f },  // soft thump
    { "Kick_05", 0, 0.50f,  7.0f, -0.85f, 600.0f, 200.0f, 0.75f, 0.60f, 0.00f, 0.30f, 2000.0f, 0.50f },  // deep techno boom
    { "Kick_06", 6, 0.45f, 16.0f, -0.80f, 200.0f,  70.0f, 0.80f, 0.40f, 0.08f, 0.35f, 1500.0f, 0.60f },  // lo-fi, felt
    { "Kick_07", 2, 0.40f, 10.0f, -0.70f, 520.0f, 150.0f, 0.60f, 0.75f, 0.00f, 0.10f, 1000.0f, 0.35f },  // sub kick
    { "Kick_08", 5, 0.50f, 22.0f, -0.85f, 300.0f,  90.0f, 0.80f, 0.50f, 0.05f, 0.20f, 3000.0f, 0.70f },  // punchy deep house
    { "Kick_09", 3, 0.45f,  8.0f, -0.75f, 380.0f, 110.0f, 0.80f, 0.50f, 0.10f, 0.20f, 1600.0f, 0.45f },  // dub
    { "Kick_10", 7, 0.50f, 28.0f, -0.90f, 140.0f,  50.0f, 0.85f, 0.35f, 0.00f, 0.15f, 3500.0f, 0.80f },  // minimal tick
};
static void setP (GranulumProcessor& p, const char* id, float v)
{
    if (auto* q = p.apvts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v));
    else printf ("no param %s\n", id);
}
static void render (GranulumProcessor& p, AudioBuffer<float>& all, int hits)
{
    const double sr = 44100.0; const int block = 128, beat = (int) (sr * 0.5), total = beat * hits + beat;
    all.setSize (2, total); all.clear();
    AudioBuffer<float> buf (2, block); MidiBuffer midi;
    for (int pos = 0; pos < total; pos += block)
    {
        midi.clear(); buf.clear();
        for (int n = 0; n < hits; ++n)
        {
            const int on = n * beat, off = on + (int) (beat * 0.6);
            if (off >= pos && off < pos + block) midi.addEvent (MidiMessage::noteOff (1, 52), off - pos);
            if (on >= pos && on < pos + block) midi.addEvent (MidiMessage::noteOn (1, 52, (uint8) 110), on - pos);
        }
        p.processBlock (buf, midi);
        const int len = jmin (block, total - pos);
        for (int c = 0; c < 2; ++c) all.copyFrom (c, pos, buf, c, 0, len);
    }
}
int main (int, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const double sr = 44100.0;
    File out (argv[1]); auto dir = out.getChildFile ("Kicks"); dir.createDirectory();
    AudioBuffer<float> sheet (2, 0);
    for (auto& k : kicks)
    {
        GranulumProcessor p; p.setRateAndBufferSizeDetails (sr, 128); p.prepareToPlay (sr, 128);
        // body: one sine stick (+ a touch of the 2nd), no grains anywhere, mono, no FX tails
        std::array<float, granulum::kNumHarmonics> h {}; h[0] = 1.0f; h[1] = k.h2; p.setHarmonics (h);
        for (auto* id : { "osc1Grain", "osc2On", "osc2Grain", "osc3Grain", "delayOn", "revOn", "filter2On", "compOn", "lfo3On" }) setP (p, id, 0.0f);
        setP (p, "osc1On", 1); setP (p, "osc1Mode", 0); setP (p, "osc1Oct", -2); setP (p, "osc1Semi", (float) k.semi); setP (p, "osc1Vol", k.osc1);
        setP (p, "osc1Voices", 1); setP (p, "osc1Air", 0); setP (p, "osc1WaveMix", 0); setP (p, "chord", 0);
        setP (p, "subOn", 1); setP (p, "subOct", -2); setP (p, "subSemi", (float) k.semi); setP (p, "subVol", k.sub);
        setP (p, "subH1", 1); setP (p, "subH2", 0); setP (p, "subH3", 0); setP (p, "subH4", 0); setP (p, "subH5", 0);
        setP (p, "mono", 1); setP (p, "porta", 0);
        setP (p, "ampOn", 1); setP (p, "ampA", 0); setP (p, "ampD", k.decay); setP (p, "ampS", 0); setP (p, "ampR", k.rel);
        // pitch drop: LFO 1 ENV on PITCH (+ up to 2 octaves at the hit, falling in 1/rate seconds)
        setP (p, "lfo1On", 1); setP (p, "lfo1Sync", 0); setP (p, "lfo1Rate", k.pRate); setP (p, "lfo1Mode", 2); setP (p, "lfo1Rise", 0); setP (p, "lfo1Grains", 0);
        p.setLfoPoints (0, { LfoPoint { 0.0f, 1.0f, k.pCurve }, { 1.0f, 0.0f, 0.0f } });
        p.setModulation (mtPitch, 0, k.pDepth);
        // click: white noise only at the hit, through the same soft low-pass
        setP (p, "osc3On", k.click > 0 ? 1.0f : 0.0f); setP (p, "osc3Type", 0); setP (p, "osc3Pan", 0); setP (p, "osc3Vol", 0); setP (p, "osc3Color", 0);
        setP (p, "lfo2On", 1); setP (p, "lfo2Sync", 0); setP (p, "lfo2Rate", 25.0f); setP (p, "lfo2Mode", 2); setP (p, "lfo2Rise", 0); setP (p, "lfo2Grains", 0);
        p.setLfoPoints (1, { LfoPoint { 0.0f, 1.0f, -0.7f }, { 0.45f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } });
        if (k.click > 0) p.setModulation (mtOsc3Vol, 1, 1.0f);
        // the beater: overtones that flash for a few ms at the hit (LFO 2) and fall with the pitch - the knock you hear first
        for (int hk : { 1, 2, 3, 4, 6 }) p.setModulation (kHarmTarget0 + hk, 1, k.click * (hk == 1 ? 0.9f : hk == 2 ? 0.8f : 0.6f));
        // warmth and softness
        setP (p, "filterOn", 1); setP (p, "ftype", 0); setP (p, "cutoff", k.cutoff); setP (p, "reso", 0.707f);
        setP (p, "filterR1", 1); setP (p, "filterR2", 1); setP (p, "filterR3", 0); setP (p, "filterR4", 0);   // the click skips the low-pass
        p.setModulation (mtCutoff, 0, 0.35f);   // the hit opens the filter for a few ms: a bright front
        setP (p, "distOn", 1); setP (p, "distType", 1); setP (p, "distDrive", k.drive); setP (p, "distMix", 1);
        setP (p, "distR1", 1); setP (p, "distR2", 1); setP (p, "distR3", 0); setP (p, "distR4", 1);   // the click stays clean (no TUBE)
        // a high shelf lifts only the click (the body is low-passed)
        setP (p, "eqOn", 1); setP (p, "eq5On", 1); setP (p, "eq5Type", 4); setP (p, "eq5Freq", 3000.0f); setP (p, "eq5Gain", 3.0f + 9.0f * k.click); setP (p, "eq5Q", 0.71f);
        setP (p, "clipOn", 1); setP (p, "pitch", 0); setP (p, "output", 0);
        if (auto* mm = MessageManager::getInstanceWithoutCreating()) mm->runDispatchLoopUntil (20);
        AudioBuffer<float> a;
        render (p, a, 4);
        const float pk = a.getMagnitude (0, a.getNumSamples());
        const float gain = jlimit (-24.0f, 24.0f, -1.0f - Decibels::gainToDecibels (pk));
        setP (p, "output", gain);
        if (auto* mm = MessageManager::getInstanceWithoutCreating()) mm->runDispatchLoopUntil (20);
        // fresh voices for the final render
        GranulumProcessor& q = p;
        render (q, a, 4);
        // fundamental at the end of a hit
        const int beat = (int) (sr * 0.5);
        double best = 0, bestHz = 0;
        for (double hz = 30; hz < 90; hz += 0.25)
        {
            double re = 0, im = 0;
            for (int i = (int) (sr * 0.12); i < (int) (sr * 0.12) + 4096 && i < beat; ++i) { const double s = a.getSample (0, i); re += s * std::cos (2 * MathConstants<double>::pi * hz * i / sr); im += s * std::sin (2 * MathConstants<double>::pi * hz * i / sr); }
            if (re * re + im * im > best) { best = re * re + im * im; bestHz = hz; }
        }
        printf ("%s  output %+5.1f dB  peak %.2f dB  body ~%.1f Hz\n", k.name, gain, Decibels::gainToDecibels (a.getMagnitude (0, a.getNumSamples())), bestHz);
        p.savePreset (k.name, dir);
        const int at = sheet.getNumSamples();
        sheet.setSize (2, at + a.getNumSamples() + (int) (sr * 0.3), true, true);
        for (int c = 0; c < 2; ++c) sheet.copyFrom (c, at, a, c, 0, a.getNumSamples());
    }
    WavAudioFormat w; auto f = out.getChildFile ("GRAIN-FORM-Kicks-01-10.wav"); f.deleteFile();
    if (auto* wr = w.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0)) { wr->writeFromAudioSampleBuffer (sheet, 0, sheet.getNumSamples()); delete wr; }
}
