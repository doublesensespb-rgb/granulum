#pragma once

#include "PluginEditor.h"

// The main screen: 16 big harmonic bars you draw with the mouse, with the sound
// "living" around them — glowing result curve, flying grain shards, aurora, sub glow.
class Organism final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit Organism (GranulumProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    bool fileHover = false;

private:
    struct Particle
    {
        float x, y, vx, vy, life, maxLife, size, rot, vrot, seed;
        juce::Colour col;
        int kind;   // 0 orb, 1 diamond, 2 shard
    };

    void timerCallback() override;
    void spawn (int layer, int count);
    juce::Rectangle<float> barArea() const;
    juce::Rectangle<float> slot (int k) const;
    int barAt (float x) const;
    void applyMouse (const juce::MouseEvent&, bool first);
    float param (const char* id) const;      // live (modulated) real value
    void computeResult();

    GranulumProcessor& proc;
    std::vector<Particle> parts;
    juce::Random rng;
    std::array<float, granulum::kNumHarmonics> bars {}, result {};
    uint32_t lastEv[2] {}, lastNotes = 0;
    float t = 0.0f, flash = 0.0f, subGlow = 0.0f, addGlow = 0.0f, smpGlow = 0.0f, energy = 0.0f;
    int lastBar = -1, hoverBar = -1;
    float lastValue = 0.0f;
    bool editing = false, erasing = false;
    juce::Image backdrop;
};

// Serum-style drawable LFO shape
class LfoEditor final : public juce::Component, private juce::Timer
{
public:
    explicit LfoEditor (GranulumProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    int lfo = 0;
    int grid = 8;
private:
    void timerCallback() override { repaint(); }
    juce::Rectangle<float> area() const;
    juce::Point<float> toScreen (float x, float y) const;
    juce::Point<float> handlePos (size_t seg) const;
    int hitPoint (juce::Point<float>) const;
    int hitHandle (juce::Point<float>) const;
    float snapX (float x) const;

    GranulumProcessor& proc;
    std::vector<LfoPoint> pts;
    int dragPoint = -1, dragHandle = -1, hover = -1, hoverHandle = -1;
    float handleStartCurve = 0.0f, dragStartY = 0.0f;
};
