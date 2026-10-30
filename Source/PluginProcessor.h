#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "GrainEngine.h"

namespace ParamID
{
    inline constexpr const char* attack      = "gAttack";
    inline constexpr const char* hold        = "hold";
    inline constexpr const char* spacing     = "spacing";
    inline constexpr const char* waveSpacing = "waveSpacing";
    inline constexpr const char* position    = "position";
    inline constexpr const char* rand        = "rand";
    inline constexpr const char* smear       = "smear";
    inline constexpr const char* pan         = "pan";
    inline constexpr const char* lfoDepth    = "lfoDepth";
    inline constexpr const char* lfoRate     = "lfoRate";
    inline constexpr const char* pitch       = "pitch";
    inline constexpr const char* pitchSpray  = "pitchSpray";
    inline constexpr const char* octave      = "octave";
    inline constexpr const char* reverse     = "reverse";
    inline constexpr const char* transients  = "transients";
    inline constexpr const char* cutoff      = "cutoff";
    inline constexpr const char* reso        = "reso";
    inline constexpr const char* ftype       = "ftype";
    inline constexpr const char* space       = "space";
    inline constexpr const char* room        = "room";
    inline constexpr const char* output      = "output";
}

struct WaveformPeaks
{
    std::vector<float> mins, maxs; // per bin, both channels merged
    std::vector<float> transients; // normalised 0..1
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
    double getTailLengthSeconds() const override { return 8.0; }

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
    void randomizeSection (int section); // 0 grains, 1 pitch, 2 filter/space

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

    std::atomic<float>* pAttack {};   std::atomic<float>* pHold {};     std::atomic<float>* pSpacing {};
    std::atomic<float>* pWave {};     std::atomic<float>* pPosition {}; std::atomic<float>* pRand {}; std::atomic<float>* pSmear {};
    std::atomic<float>* pPan {};      std::atomic<float>* pLfoDepth {}; std::atomic<float>* pLfoRate {};
    std::atomic<float>* pPitch {};    std::atomic<float>* pPitchSpray {}; std::atomic<float>* pOctave {};
    std::atomic<float>* pReverse {};  std::atomic<float>* pTrans {};
    std::atomic<float>* pCutoff {};   std::atomic<float>* pReso {};     std::atomic<float>* pFtype {};
    std::atomic<float>* pSpace {};    std::atomic<float>* pRoom {};     std::atomic<float>* pOutput {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranulumProcessor)
};
