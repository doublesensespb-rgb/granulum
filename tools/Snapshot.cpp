// Renders the plugin editor to a PNG (used by CI to preview the design).
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
        const double v = 0.5 * std::sin (2 * MathConstants<double>::pi * 110 * t + 3 * std::sin (2 * MathConstants<double>::pi * 0.7 * t))
                       + 0.2 * (r.nextFloat() * 2 - 1) * (0.3 + 0.7 * std::sin (t * 2.3));
        b.setSample (0, i, (float) (v * env));
        b.setSample (1, i, (float) (v * env * 0.9));
    }
    auto f = File::getSpecialLocation (File::tempDirectory).getChildFile ("Dusty Vox Pad.wav");
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<FileOutputStream> os (f.createOutputStream());
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
    if (w != nullptr) { os.release(); w->writeFromAudioSampleBuffer (b, 0, n); }
    return f;
}

static void pump (GranulumProcessor& p, int blocks, int ms)
{
    AudioBuffer<float> buf (2, 512);
    MidiBuffer midi;
    for (int i = 0; i < blocks; ++i)
    {
        midi.clear();
        p.processBlock (buf, midi);
    }
    MessageManager::getInstance()->runDispatchLoopUntil (ms);
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
    ed->setSize (1080, 740);

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

    pump (p, 2, 200);
    shot ("ui-empty.png");

    String err;
    p.loadSample (makeTestSample(), err);
    pump (p, 2, 100);

    // play a chord so grains appear on the screen
    p.keyboardState.noteOn (1, 60, 0.9f);
    p.keyboardState.noteOn (1, 67, 0.8f);
    if (auto* sp = p.apvts.getParameter (ParamID::spray))   sp->setValueNotifyingHost (0.35f);
    if (auto* sc = p.apvts.getParameter (ParamID::scan))    sc->setValueNotifyingHost (0.6f);
    if (auto* oc = p.apvts.getParameter (ParamID::octave))  oc->setValueNotifyingHost (0.3f);
    if (auto* dn = p.apvts.getParameter (ParamID::density)) dn->setValueNotifyingHost (0.7f);
    pump (p, 120, 300);
    shot ("ui-playing.png");

    ed.reset();
    return 0;
}
