#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include <chrono>
using namespace juce;
// Rnd <count> <outdir> : RANDOM BASS n times; each: 4 E4 notes (1/4 at 120 BPM, gate 80 %), prints peak / rms / cpu, saves wav + preset
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const double sr = 44100.0; const int block = 128; const int N = atoi (argv[1]); File out (argv[2]); out.createDirectory();
    for (int k = 0; k < N; ++k)
    {
        GranulumProcessor p; p.setRateAndBufferSizeDetails (sr, block); p.prepareToPlay (sr, block);
        p.randomizeTyped (1);
        if (auto* mm = MessageManager::getInstanceWithoutCreating()) mm->runDispatchLoopUntil (30);
        const int beat = (int) (sr * 0.5), total = beat * 5;
        AudioBuffer<float> buf (2, block), all (2, total); MidiBuffer midi; double cpu = 0;
        for (int pos = 0; pos < total; pos += block)
        {
            midi.clear(); buf.clear();
            for (int n = 0; n < 4; ++n)
            {
                const int on = n * beat, off = on + (int) (beat * 0.8);
                if (off >= pos && off < pos + block) midi.addEvent (MidiMessage::noteOff (1, 64), off - pos);
                if (on >= pos && on < pos + block) midi.addEvent (MidiMessage::noteOn (1, 64, (uint8) 100), on - pos);
            }
            auto t0 = std::chrono::high_resolution_clock::now();
            p.processBlock (buf, midi);
            cpu += std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
            const int len = jmin (block, total - pos);
            for (int c = 0; c < 2; ++c) all.copyFrom (c, pos, buf, c, 0, len);
        }
        float pk = 0; double s2 = 0; for (int c = 0; c < 2; ++c) for (int i = 0; i < total; ++i) { float v = all.getSample (c, i); pk = jmax (pk, std::abs (v)); s2 += v * v; }
        const double rms = std::sqrt (s2 / (2.0 * total));
        // per-note rms spread (same note 4x)
        double nmin = 1e9, nmax = -1e9;
        for (int n = 0; n < 4; ++n) { double a = 0; for (int i = n * beat; i < n * beat + (int) (beat * 0.8); ++i) { float v = all.getSample (0, i); a += v * v; } a = 10 * std::log10 (a / (beat * 0.8) + 1e-12); nmin = jmin (nmin, a); nmax = jmax (nmax, a); }
        printf ("%02d peak %6.1f dB  rms %6.1f dB  notes spread %4.1f dB  cpu %4.1f %%\n", k, 20 * std::log10 (pk + 1e-9), 20 * std::log10 (rms + 1e-9), nmax - nmin, 100.0 * cpu / (total / sr));
        WavAudioFormat w; auto f = out.getChildFile (String::formatted ("bass_%02d.wav", k)); f.deleteFile();
        if (auto* wr = w.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0)) { wr->writeFromAudioSampleBuffer (all, 0, total); delete wr; }
        p.savePreset (String::formatted ("rnd_bass_%02d", k), out);
    }
}
