#include "saturation/TapeDrive.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace sudwalfulkaan {

void TapeDrive::Init(float sample_rate) {
    sample_rate_ = sample_rate;
    drive_ = 1.0f;
    filter_freq_ = 12000.0f;
    mix_ = 1.0f;
    filter_enabled_ = true;
    lp_state_ = 0.0f;
    UpdateFilterCoeff();
}

float TapeDrive::Process(float in) {
    float dry = in;
    
    // Apply drive/saturation
    float wet = Saturate(in * drive_);
    
    // Apply low-pass filter if enabled
    if (filter_enabled_) {
        lp_state_ += lp_coeff_ * (wet - lp_state_);
        wet = lp_state_;
    }
    
    // Apply mix
    return dry * (1.0f - mix_) + wet * mix_;
}

void TapeDrive::SetDrive(float drive) {
    drive_ = fmaxf(1.0f, drive);
}

void TapeDrive::SetFilterFreq(float freq) {
    filter_freq_ = fmaxf(20.0f, fminf(freq, 20000.0f));
    UpdateFilterCoeff();
}

void TapeDrive::SetMix(float mix) {
    mix_ = fmaxf(0.0f, fminf(mix, 1.0f));
}

void TapeDrive::SetFilterEnabled(bool enabled) {
    filter_enabled_ = enabled;
}

void TapeDrive::UpdateFilterCoeff() {
    // One-pole low-pass filter coefficient
    // fc = cutoff frequency, fs = sample rate
    // coeff = 1 - exp(-2 * pi * fc / fs)
    float omega = 2.0f * M_PI * filter_freq_ / sample_rate_;
    lp_coeff_ = 1.0f - expf(-omega);
}

float TapeDrive::Saturate(float in) {
    // Tape-style soft saturation using tanh
    // Normalize by drive to maintain consistent output level
    float normalized = in / drive_;
    
    // Apply soft saturation
    // tanh provides smooth, symmetric clipping similar to tape
    float saturated = tanhf(in);
    
    // Blend based on drive amount for more subtle effect at low drive
    float blend = (drive_ - 1.0f) / 9.0f; // 0 at drive=1, 1 at drive=10
    blend = fmaxf(0.0f, fminf(blend, 1.0f));
    
    return normalized * (1.0f - blend) + saturated * blend;
}

} // namespace sudwalfulkaan
