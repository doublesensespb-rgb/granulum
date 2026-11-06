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
        setP (ParamID::grainOn, 1.0f); setP (ParamID::osc1Grain, 1.0f); setP (ParamID::osc2Grain, 1.0f); setP (ParamID::osc3Grain, 1.0f);
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
            e->setSize (1080, 860);
            run (p, dynamic_cast<GranulumEditor*> (e.get()), 80);
            save (*e, outDir, ("theme-" + String (t) + ".png").toRawUTF8());
        }
        return 0;
    }

    std::unique_ptr<AudioProcessorEditor> base (p.createEditor());
    auto* ed = dynamic_cast<GranulumEditor*> (base.get());
    base->setSize (1080, 860);

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
    setP (ParamID::grainOn, 1.0f); setP (ParamID::osc1Grain, 1.0f); setP (ParamID::osc2Grain, 1.0f); setP (ParamID::osc3Grain, 1.0f);
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

    // design preview: "Snapshot <dir> sampler <wav>" -> OSC 2 with factory samples, then a dropped user sample
    if (argc > 3 && String (argv[2]) == "sampler")
    {
        auto dir = GranulumProcessor::factorySampleDir();
        dir.createDirectory();
        const File src (argv[3]);
        dir.deleteRecursively(); dir.createDirectory();
        for (auto n : { "Atmo 3 F", "Dust Cathedral", "Frozen Lake", "Night Harbour", "Dust Cathedral Night Choir" }) { dir.getChildFile ("Atmos").createDirectory(); src.copyFileTo (dir.getChildFile ("Atmos").getChildFile (String (n) + ".wav")); }
        for (auto n : { "Glass Rain", "Paper Wind", "Static Choir" }) { dir.getChildFile ("Textures").createDirectory(); src.copyFileTo (dir.getChildFile ("Textures").getChildFile (String (n) + ".wav")); }
        setP (ParamID::osc3On, 0.0f);
        p.loadAdjacentFactorySample (+1);
        run (p, ed, 60);
        save (*base, outDir, "sampler-factory.png");
        String e2; p.loadSample (makeTestSample(), e2);
        run (p, ed, 30);
        save (*base, outDir, "sampler-user.png");
        p.loadAdjacentFactorySample (+1);
        run (p, ed, 10);
        for (int st = 0; st < 3; ++st) { ed->previewDropState (st); run (p, ed, 2); save (*base, outDir, ("drop-" + String (st) + ".png").toRawUTF8()); }
        ed->previewDropState (0);
        ed->openSampleMenuForPreview();
        run (p, ed, 2);
        save (*base, outDir, "sounds-menu.png");
        ed->openSampleMenuForPreview (false);
        ed->previewDropState (0);
        for (int st = 1; st <= 4; ++st)
        {
            GranulumEditor::soundsStyle = st;
            std::unique_ptr<AudioProcessorEditor> e2 (p.createEditor()); e2->setSize (1080, 860);
            run (p, dynamic_cast<GranulumEditor*> (e2.get()), 3);
            save (*e2, outDir, ("sounds-style-" + String (st) + ".png").toRawUTF8());
        }
        GranulumEditor::soundsStyle = 4;
        {
            // glint animation frames of the OSC 2 strip
            std::unique_ptr<AudioProcessorEditor> e3 (p.createEditor()); e3->setSize (1080, 860);
            outDir.getChildFile ("glint").createDirectory();
            for (int f = 0; f < 60; ++f)
            {
                MessageManager::getInstance()->runDispatchLoopUntil (60);
                auto im = e3->createComponentSnapshot ({ 536, 245, 504, 60 }, true, 2.0f);
                FileOutputStream os (outDir.getChildFile ("glint").getChildFile (String::formatted ("f%03d.png", f)));
                PNGImageFormat().writeImageToStream (im, os);
            }
        }
        String e3; p.loadSample (dir.getChildFile ("Atmos").getChildFile ("Dust Cathedral Night Choir.wav"), e3);
        run (p, ed, 4);
        save (*base, outDir, "long-name.png");
        ed->previewDropState (1);
        Thread::sleep (2200); run (p, ed, 2);
        save (*base, outDir, "long-name-hover.png");
        ed->previewDropState (0);
    }

    // design preview: "Snapshot <dir> lfo8" -> an LFO with 8 hand-made points
    if (argc > 2 && String (argv[2]) == "lfo8")
    {
        p.setLfoPoints (0, { { 0, 0.1f, 0 }, { 0.12f, 0.9f, 0.4f }, { 0.25f, 0.3f, 0 }, { 0.38f, 1.0f, -0.5f }, { 0.5f, 0.0f, 0 },
                             { 0.66f, 0.7f, 0.3f }, { 0.8f, 0.2f, 0 }, { 1, 0.6f, 0 } });
        ed->repaint();
        run (p, ed, 20);
        save (*base, outDir, "lfo8.png");
    }

    // design preview: "Snapshot <dir> v098" -> SUB on, LFO on small knobs, waveform picker, CLIP off
    if (argc > 2 && String (argv[2]) == "hw") Palette::hardware = true;
    if (argc > 2 && (String (argv[2]) == "v098" || String (argv[2]) == "hw"))
    {
        setP (ParamID::subOn, 1.0f); setP (ParamID::subH2, 0.45f); setP (ParamID::subH3, 0.2f);
        setP (ParamID::osc1On, 1.0f);
        p.setHarmonics (WavePicker::shape (1));
        p.setModulation (GranulumProcessor::modIndexOf (ParamID::osc3Vol), 1, 0.5f);
        p.setModulation (GranulumProcessor::modIndexOf (ParamID::osc2Vol), 0, -0.4f);
        p.setModulation (GranulumProcessor::modIndexOf (ParamID::subVol), 2, 0.3f);
        p.setModulation (GranulumProcessor::modIndexOf (ParamID::bloom), 0, 0.4f);
        setP (ParamID::clipOn, 0.0f);
        run (p, ed, 60);
        save (*base, outDir, "v098.png");
        setP (ParamID::clipOn, 1.0f);
        run (p, ed, 10);
        save (*base, outDir, "v098-clip-on.png");
    }

    // design preview: "Snapshot <dir> loop" -> LOOP region on the grain field
    if (argc > 2 && String (argv[2]) == "loop")
    {
        setP (ParamID::loopMode, 1.0f); setP (ParamID::loopStart, 0.28f); setP (ParamID::loopEnd, 0.62f);
        setP (ParamID::position, 0.3f);
        run (p, ed, 60);
        save (*base, outDir, "loop-1.png");
        setP (ParamID::loopMode, 2.0f);
        run (p, ed, 30);
        save (*base, outDir, "loop-2.png");
    }

    // design preview: "Snapshot <dir> scope" -> filter view with a low, resonant cutoff
    if (argc > 2 && String (argv[2]) == "scope")
    {
        p.setModulation (GranulumProcessor::modIndexOf (ParamID::cutoff), 0, 0.0f);
        setP (ParamID::cutoff, 700.0f); setP (ParamID::reso, 4.5f);
        run (p, ed, 40);
        save (*base, outDir, "scope-lp.png");
        setP (ParamID::ftype, 3.0f); setP (ParamID::cutoff, 1500.0f); setP (ParamID::reso, 2.5f);   // BAND PASS
        run (p, ed, 40);
        save (*base, outDir, "scope-bp.png");
        setP (ParamID::ftype, 4.0f); setP (ParamID::cutoff, 900.0f); setP (ParamID::peakGain, 12.0f);   // PEAK
        run (p, ed, 40);
        save (*base, outDir, "scope-peak.png");
        setP (ParamID::peakGain, -12.0f);
        run (p, ed, 40);
        save (*base, outDir, "scope-peak-cut.png");
        ed->selectFx (0);
    }

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

    // design preview: "Snapshot <dir> remove" -> LFO 1 screen with its REMOVE button (normal + hover)
    if (argc > 2 && String (argv[2]) == "remove")
    {
        p.setModulation (GranulumProcessor::modIndexOf (ParamID::bloom), 0, 0.4f);
        for (int st = 0; st < 2; ++st)
        {
            GranulumEditor::removeStyle = st;
            ed->resized();
            run (p, ed, 6);
            save (*base, outDir, ("remove-ui" + String (st) + ".png").toRawUTF8());
        }
        return 0;
    }

    // design preview: "Snapshot <dir> amp" -> AMP tab with an ADSR, a note playing
    if (argc > 2 && String (argv[2]) == "amp")
    {
        setP (ParamID::osc1On, 1.0f);
        setP (ParamID::ampD, 600.0f); setP (ParamID::ampS, 0.55f); setP (ParamID::ampR, 1500.0f);   // attack stays at its default 0
        ed->showAmp();
        p.keyboardState.noteOn (1, 48, 0.9f);
        run (p, ed, 30);
        save (*base, outDir, "amp-ui.png");
        return 0;
    }

    // design preview: "Snapshot <dir> rows" -> GRAIN row variants A..F, each: first open (hint) and in use (hover)
    if (argc > 2 && String (argv[2]) == "rows")
    {
        setP (ParamID::osc1On, 1.0f); setP (ParamID::osc1Grain, 1.0f);
        auto& he = ed->harmonicEditor();
        const auto area = ed->getLocalArea (&he, he.getLocalBounds()).expanded (8, 8);
        auto grab = [&] (const String& name)
        {
            for (int f = 0; f < 8; ++f) MessageManager::getInstance()->runDispatchLoopUntil (30);
            he.repaint();
            MessageManager::getInstance()->runDispatchLoopUntil (40);
            auto img = base->createComponentSnapshot (area, true, 2.0f);
            PNGImageFormat png;
            outDir.getChildFile (name).deleteFile();
            FileOutputStream os (outDir.getChildFile (name));
            png.writeImageToStream (img, os);
        };
        for (int st = 10; st <= 15; ++st)
        {
            he.rowStyle = st;
            for (int hs = 0; hs < 2; ++hs)
            {
                he.hintStyle = hs;
                p.setHarmonics (WavePicker::shape (0));
                p.setGrainMask (0x0000u);
                he.showHint = true; he.hoverBar = -1; he.introT = 5.0f;
                grab ("row" + String (st) + "-init-hint" + String (hs) + ".png");
            }
            p.setHarmonics (WavePicker::shape (1));
            p.setGrainMask (0xffc4u);
            he.showHint = false; he.hoverBar = 4;
            grab ("row" + String (st) + "-use.png");
        }
        return 0;
    }

    // design preview: "Snapshot <dir> select" -> intro wave over the dots, first-run hint, hover look
    if (argc > 2 && String (argv[2]) == "select")
    {
        setP (ParamID::osc1On, 1.0f); setP (ParamID::grainOn, 1.0f); setP (ParamID::osc1Grain, 1.0f); setP (ParamID::osc2Grain, 1.0f); setP (ParamID::osc3Grain, 1.0f);
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
