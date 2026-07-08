#include "modulation/StereoPhaser.h"
#include "utils/Mapping.h"

using namespace ninetysix;

// The DaisySP oscillator this LFO replaces ran at its default amplitude of
// 0.5; keep that scale so the sweep range stays identical.
static constexpr float kLfoAmp = 0.5f;

void StereoPhaser::Init(float sample_rate)
{
    sample_rate_ = sample_rate;

    lfoL_.Init(sample_rate);
    lfoR_.Init(sample_rate);

    SetFreq(freq_);
    SetFeedback(feedback_);
    SetDepth(depth_);
    SetMix(mix_);
}

void StereoPhaser::SetDelays(float* buffer, size_t size)
{
    delay_buffer_ = buffer;
    delay_size_   = size / 2; // Split buffer between L and R
    write_pos_L_  = 0;
    write_pos_R_  = 0;

    // Initialize buffer to zero to prevent garbage audio
    for (size_t i = 0; i < size; i++) {
        buffer[i] = 0.0f;
    }
}

void StereoPhaser::SetFreq(float freq)
{
    freq_ = freq;
    lfoL_.SetFreq(freq_);
    lfoR_.SetFreq(freq_ * 1.05f);
}

void StereoPhaser::SetFeedback(float feedback)
{
    feedback_ = Clamp(feedback, 0.0f, 0.95f);
}

void StereoPhaser::SetDepth(float depth)
{
    depth_ = Clamp(depth, 0.0f, 1.0f);
}

void StereoPhaser::SetMix(float mix)
{
    mix_ = Clamp(mix, 0.0f, 1.0f);
}

float StereoPhaser::ProcessLeft(float in)
{
    if (!delay_buffer_ || delay_size_ == 0) {
        return in; // Safety check
    }

    float mod       = (lfoL_.ProcessSine() * kLfoAmp + 1.0f) * 0.5f;
    float delayTime = 1.0f + mod * depth_ * (float(delay_size_) * 0.25f - 2.0f);

    // Calculate read position for left channel
    size_t read_pos = (write_pos_L_ + delay_size_ - (size_t)delayTime) % delay_size_;

    float fb  = in + prevL_ * feedback_;
    float wet = delay_buffer_[read_pos];
    delay_buffer_[write_pos_L_] = fb;

    write_pos_L_ = (write_pos_L_ + 1) % delay_size_;
    prevL_ = wet;

    return in * (1.0f - mix_) + wet * mix_;
}

float StereoPhaser::ProcessRight(float in)
{
    if (!delay_buffer_ || delay_size_ == 0) {
        return in; // Safety check
    }

    float mod       = (lfoR_.ProcessSine() * kLfoAmp + 1.0f) * 0.5f;
    float delayTime = 1.0f + mod * depth_ * (float(delay_size_) * 0.25f - 2.0f);

    // Calculate read position for right channel (offset by half buffer)
    size_t read_pos = (write_pos_R_ + delay_size_ - (size_t)delayTime) % delay_size_ + delay_size_;

    float fb  = in + prevR_ * feedback_;
    float wet = delay_buffer_[read_pos];
    delay_buffer_[write_pos_R_ + delay_size_] = fb;

    write_pos_R_ = (write_pos_R_ + 1) % delay_size_;
    prevR_ = wet;

    return in * (1.0f - mix_) + wet * mix_;
}
