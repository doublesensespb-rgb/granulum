#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
using namespace juce;
int main (int, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    GranulumProcessor p; p.setRateAndBufferSizeDetails (44100, 128); p.prepareToPlay (44100, 128);
    printf ("fresh classic=%d\n", (int) p.classicFilter.load());
    p.loadPreset (File (argv[1]));
    printf ("after load classic=%d key=%g key2=%g\n", (int) p.classicFilter.load(), p.apvts.getRawParameterValue ("filterKey")->load(), p.apvts.getRawParameterValue ("filterKey2")->load());
    if (auto* mm = MessageManager::getInstanceWithoutCreating()) mm->runDispatchLoopUntil (30);
    printf ("after loop classic=%d\n", (int) p.classicFilter.load());
}
