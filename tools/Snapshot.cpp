// Renders the plugin editor to PNGs (used by CI to preview the design).
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>

using namespace juce;

static File makeTestSample()
{
    const double sr = 44100.0;
    const int n = (int) (sr * 4.0);
    AudioBuffer<float> b (2, n);
    Random r (7);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const double env = std::pow (std::sin (MathConstants<double>::pi * std::fmod (t * 1.5, 1.0)), 3.0);
        const double v = 0.5 * std::sin (2 * MathConstants<double>::pi * 220 * t + 3 * std::sin (2 * MathConstants<double>::pi * 0.7 * t))
                       + 0.2 * (r.nextFloat() * 2 - 1) * (0.3 + 0.7 * std::sin (t * 2.3));
        b.setSample (0, i, (float) (v * env));
        b.setSample (1, i, (float) (v * env * 0.9));
    }
    auto f = File::getSpecialLocation (File::tempDirectory).getChildFile ("Vocal Chop.wav");
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<FileOutputStream> os (f.createOutputStream());
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
    if (w != nullptr) { os.release(); w->writeFromAudioSampleBuffer (b, 0, n); }
    return f;
}

// run audio and the UI together for a while so shards are born and fly
static void run (GranulumProcessor& p, GranulumEditor* ed, int frames)
{
    AudioBuffer<float> buf (2, 441);
    MidiBuffer midi;
    for (int f = 0; f < frames; ++f)
    {
        for (int i = 0; i < 2; ++i) { midi.clear(); buf.clear(); p.processBlock (buf, midi); }
        if (ed != nullptr) ed->grainField().simulate (0.02f);
        MessageManager::getInstance()->runDispatchLoopUntil (2);
    }
}

static void save (Component& c, const File& dir, const char* name)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    PNGImageFormat png;
    auto f = dir.getChildFile (name);
    f.deleteFile();
    FileOutputStream os (f);
    png.writeImageToStream (img, os);
    std::cout << "wrote " << f.getFullPathName() << std::endl;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File outDir (argc > 1 ? String (argv[1]) : File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    GranulumProcessor p;
    p.setPlayConfigDetails (0, 2, 44100.0, 441);
    p.prepareToPlay (44100.0, 441);

    std::unique_ptr<AudioProcessorEditor> base (p.createEditor());
    auto* ed = dynamic_cast<GranulumEditor*> (base.get());
    base->setSize (1080, 800);

    auto setP = [&p] (const char* id, float v) { if (auto* x = p.apvts.getParameter (id)) x->setValueNotifyingHost (x->convertTo0to1 (v)); };

    // 0) INIT: how the plugin opens
    p.rebuildNow();
    run (p, ed, 10);
    save (*base, outDir, "ui-init.png");

    // 1) additive + chord + granular cloud
    std::array<float, granulum::kNumHarmonics> h {};
    const float bars[] = { 1.0f, 0.55f, 0.3f, 0.62f, 0.12f, 0.4f, 0.0f, 0.28f, 0.08f, 0.2f, 0.0f, 0.15f, 0.05f, 0.1f, 0.0f, 0.06f };
    for (size_t i = 0; i < std::size (bars); ++i) h[i] = bars[i];
    p.setHarmonics (h);
    p.rebuildNow();
    setP (ParamID::chord, 3.0f);         // MAJ7
    setP (ParamID::grainOn, 1.0f);
    setP (ParamID::bloom, 0.55f);
    setP (ParamID::smear, 0.35f);
    setP (ParamID::scatter, 0.6f);
    setP (ParamID::shimmer, 0.4f);
    setP (ParamID::drift, 0.5f);
    setP (ParamID::density, 0.6f);
    setP (ParamID::filterOn, 1.0f);
    setP (ParamID::cutoff, 1800.0f);
    setP (ParamID::revOn, 1.0f);
    p.setModulation (GranulumProcessor::modIndexOf (ParamID::cutoff), 0, 0.3f);
    p.setModulation (GranulumProcessor::modIndexOf (ParamID::smear), 1, 0.25f);
    p.setLfoPoints (0, GranulumProcessor::lfoPreset (0));
    p.keyboardState.noteOn (1, 48, 0.9f);
    run (p, ed, 90);
    save (*base, outDir, "ui-add.png");
    p.keyboardState.noteOff (1, 48, 0.0f);
    run (p, ed, 20);

    // 2) sample through the grains, long smeared strokes
    String err;
    p.loadSample (makeTestSample(), err);
    setP (ParamID::osc1On, 0.0f);
    setP (ParamID::chord, 0.0f);
    setP (ParamID::smear, 0.8f);
    setP (ParamID::shimmer, 0.7f);
    setP (ParamID::position, 0.2f);
    setP (ParamID::distOn, 1.0f);
    p.rebuildNow();
    run (p, ed, 4);
    p.keyboardState.noteOn (1, 52, 0.9f);
    run (p, ed, 90);
    save (*base, outDir, "ui-playing.png");

    // 3) the chord popup on its own
    {
        setP (ParamID::chord, 5.0f);   // MAJ9
        ChordPicker picker (p);
        picker.setLookAndFeel (&ed->lookAndFeel());
        struct Bg : Component
        {
            void paint (Graphics& g) override { g.fillAll (Palette::panel); }
        } bg;
        bg.setSize (picker.getWidth() + 32, picker.getHeight() + 32);
        bg.addAndMakeVisible (picker);
        picker.setTopLeftPosition (16, 16);
        save (bg, outDir, "ui-chords.png");
        picker.setLookAndFeel (nullptr);
    }

    base.reset();
    return 0;
}
