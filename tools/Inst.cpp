#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
using namespace juce;
int main()
{
    ScopedJuceInitialiser_GUI init;
    GranulumProcessor p;
    auto root = GranulumProcessor::presetFolder();
    printf ("root %s\n", root.getFullPathName().toRawUTF8());
    for (auto& f : root.findChildFiles (File::findFiles, true, "*.gpreset")) printf ("%s\n", f.getRelativePathFrom (root).toRawUTF8());
}
