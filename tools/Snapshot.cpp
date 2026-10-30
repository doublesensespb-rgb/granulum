// Renders the plugin editor to PNGs (used by CI to preview the design).
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>

using namespace juce;

static File makeTestSample()
{
    const double sr = 44100.0;
    const int n = (int) (sr * 3.0);
    AudioBuffer<float> b (2, n);
    Random r (7);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const double env = 0.6 + 0.4 * std::sin (t * 3.0);
        double v = 0.0;
        for (int k = 1; k < 12; ++k) v += std::sin (2 * MathConstants<double>::pi * 110.0 * k * t) / k * (k == 3 || k == 5 ? 1.8 : 1.0);
        v = v * 0.3 * env + 0.05 * (r.nextFloat() * 2 - 1);
        b.setSample (0, i, (float) v);
        b.setSample (1, i, (float) v);
    }
    auto f = File::getSpecialLocation (File::tempDirectory).getChildFile ("Growl Vox.wav");
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<FileOutputStream> os (f.createOutputStream());
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
    if (w != nullptr) { os.release(); w->writeFromAudioSampleBuffer (b, 0, n); }
    return f;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File outDir (argc > 1 ? String (argv[1]) : File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    GranulumProcessor p;
    p.setPlayConfigDetails (0, 2, 44100.0, 512);
    p.prepareToPlay (44100.0, 512);

    std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (1100, 860);

    auto live = [&] (int frames)
    {
        AudioBuffer<float> buf (2, 512);
        MidiBuffer midi;
        for (int f = 0; f < frames; ++f)
        {
            for (int b = 0; b < 2; ++b) { midi.clear(); p.processBlock (buf, midi); }
            MessageManager::getInstance()->runDispatchLoopUntil (22);
        }
    };
    auto shot = [&] (const char* name)
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        PNGImageFormat png;
        auto f = outDir.getChildFile (name);
        f.deleteFile();
        FileOutputStream os (f);
        png.writeImageToStream (img, os);
        std::cout << "wrote " << f.getFullPathName() << std::endl;
    };
    auto setP = [&p] (const char* id, float v) { if (auto* x = p.apvts.getParameter (id)) x->setValueNotifyingHost (x->convertTo0to1 (v)); };

    // scene 1: growling additive bass with grains flying
    const float bars[] = { 1.0f, 0.72f, 0.2f, 0.58f, 0.12f, 0.45f, 0.05f, 0.33f, 0.0f, 0.24f, 0.0f, 0.16f, 0.0f, 0.1f, 0.0f, 0.06f };
    for (int i = 0; i < granulum::kNumHarmonics; ++i) p.setHarmonic (i, bars[i]);
    setP (ParamID::mono, 0.0f);
    setP (ParamID::tone, 5.0f);
    setP (ParamID::texture, 0.55f);
    setP (ParamID::motion, 0.45f);
    setP (ParamID::dirtDrive, 0.45f);
    setP (ParamID::dirtType, 2.0f);
    setP (ParamID::cutoff, 2500.0f);
    p.setModulation (GranulumProcessor::modIndexOf (ParamID::tone), 0, 0.3f);
    p.setModulation (GranulumProcessor::modIndexOf (ParamID::cutoff), 1, -0.25f);
    p.keyboardState.noteOn (1, 33, 1.0f);
    p.keyboardState.noteOn (1, 45, 0.9f);
    live (60);
    shot ("ui-add.png");
    p.keyboardState.noteOff (1, 33, 0.0f);
    p.keyboardState.noteOff (1, 45, 0.0f);
    live (20);

    // scene 2: sampler + add, sharp shards
    String err;
    p.loadSample (makeTestSample(), err);
    setP (ParamID::texture, 0.85f);
    setP (ParamID::tone, -4.0f);
    setP (ParamID::motion, 0.7f);
    p.keyboardState.noteOn (1, 36, 1.0f);
    live (60);
    shot ("ui-playing.png");

    ed.reset();
    return 0;
}
