#include "StereoChorus.h"
#include "Synthesis/oscillator.h"

using namespace daisysp;
using namespace sudwalfulkaan;

void StereoChorus::Init(float sample_rate) {
    sample_rate_ = sample_rate;
    delayL.Init();
    delayR.Init();

    lfoL.Init(sample_rate_);
    lfoR.Init(sample_rate_);

    lfoL.SetWaveform(Oscillator::WAVE_TRI);
    lfoR.SetWaveform(Oscillator::WAVE_TRI);

    SetRate(.5f);
    SetDepth(.005f);
    SetMix(.5f);
    SetFeedback(.3f);
    SetIntensity(.5f);
}

void StereoChorus::SetRate(float rate) {
    rate_ = rate;
    lfoL.SetFreq(rate_);
    lfoR.SetFreq(rate_ * 1.1f); // Slight detune for stereo width
}

void StereoChorus::SetDepth(float depth) {
    depth_ = depth;
}

void StereoChorus::SetMix(float mix) {
    mix_ = mix;
}

void StereoChorus::SetFeedback(float feedback) {
    feedback_ = fclamp(feedback, 0.0f, 0.95f);  // Prevent runaway feedback
}

void StereoChorus::SetIntensity(float intensity) {
    // Clamp intensity to 0.0 - 1.0 range
    intensity = fclamp(intensity, 0.0f, 1.0f);

    // Scale parameters based on intensity
    float rate = 0.1f + intensity * 4.9f;   // Rate: 0.1 Hz (slow) → 5 Hz (fast)
    float depth = 0.002f + intensity * 0.008f; // Depth: 2ms → 10ms
    float mix = intensity;                // Dry/Wet: 0 → 100%
    float feedback = intensity * 0.8f;    // Feedback: 0 → 80%

    // Apply computed values
    SetRate(rate);
    SetDepth(depth);
    SetMix(mix);
    SetFeedback(feedback);
}

float StereoChorus::ProcessLeft(float in) {
    float mod = lfoL.Process() * depth_ * sample_rate_;
    float delayTime = fclamp(mod + 5.0f, 1.0f, MAX_DELAY - 1.0f);
    delayL.SetDelay(delayTime);

    float wet = delayL.Read();
    delayL.Write(in);

    return (1.0f - mix_) * in + mix_ * wet;
}

float StereoChorus::ProcessRight(float in) {
    float mod = lfoR.Process() * depth_ * sample_rate_;
    float delayTime = fclamp(mod + 5.0f, 1.0f, MAX_DELAY - 1.0f);
    delayR.SetDelay(delayTime);

    float wet = delayR.Read();
    delayR.Write(in);

    return (1.0f - mix_) * in + mix_ * wet;
}