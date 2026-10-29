#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "GrainEngine.h"

namespace ParamID
{
    inline constexpr const char* position   = "position";
    inline constexpr const char* spray      = "spray";
    inline constexpr const char* scan       = "scan";
    inline constexpr const char* size       = "size";
    inline constexpr const char* density    = "density";
    inline constexpr const char* chaos      = "chaos";
    inline constexpr const char* pitch      = "pitch";
    inline constexpr const char* pitchSpray = "pitchSpray";
    inline constexpr const char* octave     = "octave";
    inline constexpr const char* reverse    = "reverse";
    inline constexpr const char* shape      = "shape";
    inline constexpr const char* spread     = "spread";
    inline constexpr const char* attack     = "attack";
    inline constexpr const char* release    = "release";
    inline constexpr const char* cutoff     = "cutoff";
    inline constexpr const char* reso       = "reso";
    inline constexpr const char* ftype      = "ftype";
    inline constexpr const char* space      = "space";
    inline constexpr const char* room       = "room";
    inline constexpr const char* output     = "output";
}

struct WaveformPeaks
{
    std::vector<float> mins, maxs; // per bin, both channels merged
};

class GranulumProcessor final : public juce::AudioProcessor,
                                private juce::Timer
{
public:
    GranulumProcessor();
    ~GranulumProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Granulum"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- sample handling (message thread) ----
    bool loadSample (const juce::File& file, juce::String& error);
    juce::String getSampleName() const;
    std::shared_ptr<const WaveformPeaks> getPeaks() const;
    double getSampleLengthSeconds() const;
    static bool isSupportedAudioFile (const juce::String& path);

    void randomize();

    juce::AudioProcessorValueTreeState apvts;
    granulum::GrainEngine engine;
    juce::MidiKeyboardState keyboardState;

    std::atomic<float> outputLevel { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    void readParams (granulum::Params&) const;

    juce::AudioFormatManager formatManager;

    // sample hand-over between threads
    juce::SpinLock sampleLock;
    std::shared_ptr<granulum::SampleData> pendingSample;   // set by message thread
    std::shared_ptr<granulum::SampleData> retiredSample;   // freed by message thread
    std::shared_ptr<granulum::SampleData> activeSample;    // owned by audio thread

    mutable juce::CriticalSection infoLock;
    juce::File samplePath;
    std::shared_ptr<const WaveformPeaks> peaks;
    double sampleSeconds = 0.0;

    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::Reverb reverb;
    juce::SmoothedValue<float> outGain;
    juce::AudioBuffer<float> work;

    std::atomic<float>* pPosition {}; std::atomic<float>* pSpray {};  std::atomic<float>* pScan {};
    std::atomic<float>* pSize {};     std::atomic<float>* pDensity {}; std::atomic<float>* pChaos {};
    std::atomic<float>* pPitch {};    std::atomic<float>* pPitchSpray {}; std::atomic<float>* pOctave {};
    std::atomic<float>* pReverse {};  std::atomic<float>* pShape {};  std::atomic<float>* pSpread {};
    std::atomic<float>* pAttack {};   std::atomic<float>* pRelease {}; std::atomic<float>* pCutoff {};
    std::atomic<float>* pReso {};     std::atomic<float>* pFtype {};  std::atomic<float>* pSpace {};
    std::atomic<float>* pRoom {};     std::atomic<float>* pOutput {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranulumProcessor)
};
