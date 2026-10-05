#include "filters/Resonator.h"

#include <math.h>

using namespace ninetysix;

namespace {
    constexpr float kMinFreq       = 20.0f;
    constexpr float kMaxFreq       = 4000.0f;
    // Right channel a few cents sharp: the two rings beat slowly for width
    constexpr float kDetuneRight   = 1.004f;
    constexpr float kInputCutHz    = 80.0f;
    constexpr float kDampHz        = 6000.0f;
    // Feedback ceiling: keeps the loop stable at the highest pitches,
    // where a fixed ring time would ask for more
    constexpr float kMaxFeedback   = 0.995f;
    // Level compensation: at higher pitches the harmonics sit further
    // apart, and on bass-heavy music less of the input lands on them. +3 dB
    // per octave above 110 Hz (up to +12 dB) keeps the ring about as loud
    // across the sweep (measured on low-passed noise)
    constexpr float kCompRefHz     = 110.0f;
    constexpr float kMaxComp       = 4.0f;
    constexpr float kSmoothSeconds = 0.01f;
    constexpr float kGlideSeconds  = 0.03f;
    constexpr float kLn1000        = 6.90775527898f; // 60 dB
    constexpr float kTwoPi         = 6.28318530718f;
}

void Resonator::Init(float sample_rate) {
    sample_rate_ = sample_rate;
    smooth_      = 1.0f - expf(-1.0f / (kSmoothSeconds * sample_rate));
    glide_       = 1.0f - expf(-1.0f / (kGlideSeconds * sample_rate));
    in_cut_coef_ = 1.0f - expf(-kTwoPi * kInputCutHz / sample_rate);
    damp_coef_   = 1.0f - expf(-kTwoPi * kDampHz / sample_rate);
    Reset();
    wet_ = mix_;
}

void Resonator::Reset() {
    line_l_.Init();
    line_r_.Init();
    lo_l_ = lo_r_ = lp_l_ = lp_r_ = 0.0f;
    freq_ = target_freq_;
    wet_  = 0.0f;
}

void Resonator::SetFreq(float hz) {
    target_freq_ = hz < kMinFreq ? kMinFreq : (hz > kMaxFreq ? kMaxFreq : hz);
}

void Resonator::SetRingTime(float seconds) {
    ring_time_ = seconds < 0.01f ? 0.01f : seconds;
}

void Resonator::SetMix(float mix) {
    mix_ = mix < 0.0f ? 0.0f : (mix > 1.0f ? 1.0f : mix);
}

float Resonator::Channel(float in, float delay, float feedback, float norm,
                         DelayLine<kMaxDelay>& line, float& lo, float& lp) {
    lo += in_cut_coef_ * (in - lo);
    line.SetDelay(delay);
    const float ring = line.Read();
    lp += damp_coef_ * (ring - lp);
    line.Write((in - lo) * norm + feedback * lp);
    return ring;
}

void Resonator::Process(float& left, float& right) {
    freq_ += glide_ * (target_freq_ - freq_);
    wet_  += smooth_ * (mix_ - wet_);

    // Loop period = delay + the damping filter's delay (~1 sample), so
    // take that off to stay in tune at high pitches
    const float damp_delay = (1.0f - damp_coef_) / damp_coef_;
    const float period     = sample_rate_ / freq_;
    float delay_l = period - damp_delay;
    float delay_r = period / kDetuneRight - damp_delay;
    if (delay_l < 1.0f) delay_l = 1.0f;
    if (delay_r < 1.0f) delay_r = 1.0f;

    // Same ring time at any pitch: -60 dB after ring_time_ seconds, i.e.
    // after ring_time_ * freq passes through the loop
    float feedback = expf(-kLn1000 * period / (sample_rate_ * ring_time_));
    if (feedback > kMaxFeedback) feedback = kMaxFeedback;
    // Same loudness at any feedback: sqrt(1 - g^2) is unity power gain for
    // a comb fed flat-spectrum input (the harmonics ring above it)
    const float norm = sqrtf(1.0f - feedback * feedback);

    float comp = sqrtf(freq_ / kCompRefHz);
    comp = comp < 1.0f ? 1.0f : (comp > kMaxComp ? kMaxComp : comp);

    const float ring_l = Channel(left, delay_l, feedback, norm, line_l_, lo_l_, lp_l_);
    const float ring_r = Channel(right, delay_r, feedback, norm, line_r_, lo_r_, lp_r_);
    left  += wet_ * comp * ring_l;
    right += wet_ * comp * ring_r;
}
