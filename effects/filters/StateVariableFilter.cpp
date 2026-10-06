#include "filters/StateVariableFilter.h"
#include "utils/Mapping.h"

#include <math.h>

using namespace ninetysix;

namespace {
    constexpr float kPi = 3.1415927410125732421875f;
}

void StateVariableFilter::Init(float sample_rate) {
    sample_rate_ = sample_rate;
    fc_max_ = sample_rate / 3.0f;
    res_ = 0.5f;
    drive_ = 0.5f;
    pre_drive_ = 0.5f;
    freq_ = 0.25f;
    damp_ = 0.0f;
    low_ = band_ = 0.0f;
    out_low_ = out_high_ = out_band_ = out_notch_ = out_peak_ = 0.0f;
}

// Two passes per sample (double sampling keeps it stable up to fs / 3);
// each output is the average of both
void StateVariableFilter::Process(float in) {
    float notch = in - damp_ * band_;
    low_ = low_ + freq_ * band_;
    float high = notch - low_;
    band_ = freq_ * high + band_ - drive_ * band_ * band_ * band_;
    out_low_   = 0.5f * low_;
    out_high_  = 0.5f * high;
    out_band_  = 0.5f * band_;
    out_peak_  = 0.5f * (low_ - high);
    out_notch_ = 0.5f * notch;

    notch = in - damp_ * band_;
    low_ = low_ + freq_ * band_;
    high = notch - low_;
    band_ = freq_ * high + band_ - drive_ * band_ * band_ * band_;
    out_low_   += 0.5f * low_;
    out_high_  += 0.5f * high;
    out_band_  += 0.5f * band_;
    out_peak_  += 0.5f * (low_ - high);
    out_notch_ += 0.5f * notch;
}

void StateVariableFilter::SetFreq(float hz) {
    const float fc = Clamp(hz, 1.0e-6f, fc_max_);
    // Twice the sample rate: the filter runs two passes per sample
    freq_ = 2.0f * sinf(kPi * fminf(0.25f, fc / (sample_rate_ * 2.0f)));
    UpdateDamp();
}

void StateVariableFilter::SetRes(float res) {
    res_ = Clamp(res, 0.0f, 1.0f);
    UpdateDamp();
    drive_ = pre_drive_ * res_;
}

void StateVariableFilter::SetDrive(float drive) {
    pre_drive_ = Clamp(drive * 0.1f, 0.0f, 1.0f);
    drive_ = pre_drive_ * res_;
}

// Resonance to damping, capped where the double-sampled loop would go
// unstable at high cutoffs
void StateVariableFilter::UpdateDamp() {
    damp_ = fminf(2.0f * (1.0f - powf(res_, 0.25f)),
                  fminf(2.0f, 2.0f / freq_ - freq_ * 0.5f));
}
