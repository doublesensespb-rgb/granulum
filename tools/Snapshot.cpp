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

    auto setP = [&p] (const char* id, float v) { if (auto* x = p.apvts.getParameter (id)) x->setValueNotifyingHost (x->convertTo0to1 (v)); };

    // design variants: "Snapshot <dir> themes" renders the same playing scene in each palette
    if (argc > 2 && String (argv[2]) == "themes")
    {
        std::array<float, granulum::kNumHarmonics> h {};
        const float bars[] = { 1.0f, 0.55f, 0.3f, 0.62f, 0.12f, 0.4f, 0.0f, 0.28f, 0.08f, 0.2f, 0.0f, 0.15f, 0.05f, 0.1f, 0.0f, 0.06f };
        for (size_t i = 0; i < std::size (bars); ++i) h[i] = bars[i];
        p.setHarmonics (h);
        p.rebuildNow();
        String err;
        p.loadSample (makeTestSample(), err);
        setP (ParamID::osc2Vol, 0.5f);
        setP (ParamID::chord, 3.0f);
        setP (ParamID::grainOn, 1.0f);
        setP (ParamID::bloom, 0.5f);  setP (ParamID::smear, 0.45f); setP (ParamID::scatter, 0.6f);
        setP (ParamID::shimmer, 0.45f); setP (ParamID::drift, 0.5f); setP (ParamID::density, 0.55f);
        setP (ParamID::filterOn, 1.0f); setP (ParamID::cutoff, 1800.0f); setP (ParamID::revOn, 1.0f); setP (ParamID::distOn, 1.0f);
        p.setModulation (GranulumProcessor::modIndexOf (ParamID::cutoff), 0, 0.3f);
        p.setModulation (GranulumProcessor::modIndexOf (ParamID::smear), 1, 0.25f);
        p.keyboardState.noteOn (1, 52, 0.9f);
        const int first = argc > 3 ? String (argv[3]).getIntValue() : 1;
        const int last  = argc > 4 ? String (argv[4]).getIntValue() : 4;
        for (int t = first; t <= last; ++t)
        {
            Palette::applyTheme (t);
            std::unique_ptr<AudioProcessorEditor> e (p.createEditor());
            e->setSize (1080, 800);
            run (p, dynamic_cast<GranulumEditor*> (e.get()), 80);
            save (*e, outDir, ("theme-" + String (t) + ".png").toRawUTF8());
        }
        return 0;
    }

    std::unique_ptr<AudioProcessorEditor> base (p.createEditor());
    auto* ed = dynamic_cast<GranulumEditor*> (base.get());
    base->setSize (1080, 800);

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
    setP (ParamID::chime, 0.55f);
    p.setGrainMask (0xfffcu);            // harmonics 1-2 clean & mono
    setP (ParamID::lfo (1, "On").toRawUTF8(), 0.0f);   // LFO 2 switched off (routings kept)
    setP (ParamID::filter2On, 1.0f);
    setP (ParamID::ftype, 1.0f);       // LOW PASS 24
    setP (ParamID::delayOn, 1.0f);
    setP (ParamID::route ("delay", 0).toRawUTF8(), 0.0f);
    ed->selectFx (1);
    p.setModulation (GranulumProcessor::modIndexOf (ParamID::cutoff), 0, 0.3f);
    p.setModulation (GranulumProcessor::modIndexOf (ParamID::smear), 1, 0.25f);
    p.setLfoPoints (0, GranulumProcessor::lfoPreset (1));
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
    setP (ParamID::osc3On, 1.0f);
    setP (ParamID::osc3Type, 6.0f);     // PAPER
    setP (ParamID::osc3Color, 0.3f);
    setP (ParamID::route ("dist", 2).toRawUTF8(), 0.0f);
    ed->selectFx (0);
    p.rebuildNow();
    run (p, ed, 4);
    p.keyboardState.noteOn (1, 52, 0.9f);
    run (p, ed, 90);
    save (*base, outDir, "ui-playing.png");

    // design preview: "Snapshot <dir> grainrow" -> animation frames of the OSC 1 panel in both GRAIN row styles
    if (argc > 2 && String (argv[2]) == "grainrow")
    {
        p.keyboardState.noteOff (1, 52, 0.0f);
        setP (ParamID::osc1On, 1.0f);
        std::array<float, granulum::kNumHarmonics> hh {};
        const float bb[] = { 1.0f, 0.55f, 0.3f, 0.62f, 0.12f, 0.4f, 0.0f, 0.28f, 0.08f, 0.2f, 0.0f, 0.15f, 0.05f, 0.1f, 0.0f, 0.06f };
        for (size_t i = 0; i < std::size (bb); ++i) hh[i] = bb[i];
        p.setHarmonics (hh);
        p.setGrainMask (0xfffcu);
        auto& he = ed->harmonicEditor();
        const auto area = he.getBoundsInParent().expanded (6, 4).withTrimmedTop (-30).withRight (he.getBoundsInParent().getRight() + 160);
        for (int style = 0; style < 5; ++style)
        {
            HarmonicEditor::barStyle = style;
            for (int f = 0; f < 60; ++f)
            {
                MessageManager::getInstance()->runDispatchLoopUntil (42);
                auto img = base->createComponentSnapshot (area, true, 2.0f);
                PNGImageFormat png;
                auto file = outDir.getChildFile ("bar" + String (style) + "_" + String (f).paddedLeft ('0', 2) + ".png");
                FileOutputStream os (file);
                png.writeImageToStream (img, os);
            }
        }
        return 0;
    }

    // design preview: "Snapshot <dir> strings" -> whole UI with string harmonics + flat knobs
    if (argc > 2 && String (argv[2]) == "strings")
    {
        GranulumLook::flatKnobs = true;
        HarmonicEditor::barStyle = 5;
        ed->harmonicEditor().rowStyle = 2;
        base->sendLookAndFeelChange();
        setP (ParamID::osc1On, 1.0f);
        setP (ParamID::chord, 3.0f);
        p.setGrainMask (0xfffcu);
        run (p, ed, 40);
        save (*base, outDir, "strings-ui.png");
        return 0;
    }

    // design preview: "Snapshot <dir> amp" -> AMP tab with an ADSR, a note playing
    if (argc > 2 && String (argv[2]) == "amp")
    {
        setP (ParamID::osc1On, 1.0f);
        setP (ParamID::ampA, 120.0f); setP (ParamID::ampD, 600.0f); setP (ParamID::ampS, 0.55f); setP (ParamID::ampR, 1500.0f);
        ed->showAmp();
        p.keyboardState.noteOn (1, 48, 0.9f);
        run (p, ed, 30);
        save (*base, outDir, "amp-ui.png");
        return 0;
    }

    // design preview: "Snapshot <dir> select" -> intro wave over the dots, first-run hint, hover look
    if (argc > 2 && String (argv[2]) == "select")
    {
        setP (ParamID::osc1On, 1.0f); setP (ParamID::grainOn, 1.0f);
        p.setGrainMask (0xfffcu);
        auto& he = ed->harmonicEditor();
        he.showHint = true;
        he.introT = 0.0f;
        const auto area = he.getBoundsInParent().expanded (6, 4).withTrimmedTop (-30).withRight (he.getBoundsInParent().getRight() + 160);
        for (int f = 0; f < 90; ++f)
        {
            he.hoverBar = f >= 55 ? 6 : -1;
            MessageManager::getInstance()->runDispatchLoopUntil (40);
            auto img = base->createComponentSnapshot (area, true, 2.0f);
            PNGImageFormat png;
            FileOutputStream os (outDir.getChildFile ("sel" + String (f).paddedLeft ('0', 2) + ".png"));
            png.writeImageToStream (img, os);
        }
        return 0;
    }

    // design preview: "Snapshot <dir> toggles" -> pressed-button colour variants
    if (argc > 2 && String (argv[2]) == "toggles")
    {
        setP (ParamID::delayOn, 1.0f); setP (ParamID::osc1On, 1.0f); setP (ParamID::chord, 3.0f);
        ed->selectFx (1);
        for (int st = 0; st < 7; ++st)
        {
            GranulumLook::toggleStyle = st;
            base->repaint();
            MessageManager::getInstance()->runDispatchLoopUntil (80);
            auto img = base->createComponentSnapshot (base->getLocalBounds(), true, 1.0f);
            PNGImageFormat png;
            FileOutputStream os (outDir.getChildFile ("tg" + String (st) + ".png"));
            png.writeImageToStream (img, os);
        }
        return 0;
    }

    // design preview: "Snapshot <dir> digits" -> routing digits in every style
    if (argc > 2 && String (argv[2]) == "digits")
    {
        setP (ParamID::filterOn, 1.0f); setP (ParamID::delayOn, 1.0f);
        setP (ParamID::route ("filter", 1).toRawUTF8(), 0.0f);
        setP (ParamID::route ("delay", 0).toRawUTF8(), 0.0f);
        ed->selectFx (1);
        for (int st = 0; st < 5; ++st)
        {
            RouteButtons::style = st;
            base->repaint();
            MessageManager::getInstance()->runDispatchLoopUntil (80);
            auto img = base->createComponentSnapshot (Rectangle<int> (530, 372, 534, 230), true, 2.0f);
            PNGImageFormat png;
            FileOutputStream os (outDir.getChildFile ("dg" + String (st) + ".png"));
            png.writeImageToStream (img, os);
        }
        return 0;
    }

    // design preview: "Snapshot <dir> fxtabs" -> the effects block in every tab style
    if (argc > 2 && String (argv[2]) == "fxtabs")
    {
        setP (ParamID::distOn, 0.0f); setP (ParamID::delayOn, 1.0f); setP (ParamID::revOn, 1.0f);
        ed->selectFx (2);
        for (int st = 2; st < 3; ++st)
        {
            FxTab::style = st;
            base->repaint();
            MessageManager::getInstance()->runDispatchLoopUntil (60);
            auto img = base->createComponentSnapshot (Rectangle<int> (728, 372, 336, 230), true, 2.0f);
            PNGImageFormat png;
            FileOutputStream os (outDir.getChildFile ("fx" + String (st) + ".png"));
            png.writeImageToStream (img, os);
        }
        return 0;
    }

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
