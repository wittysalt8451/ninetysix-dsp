#include "LinkwitzRileyCrossover.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846 /* pi */
#endif

#include <cmath>
#include <algorithm>

using namespace sudwalfulkaan;

void LinkwitzRileyCrossover::Init(float cutoff, float sampleRate) {
    sampleRate_ = sampleRate;
    SetCutoff(cutoff);
    lowPrev1_ = lowPrev2_ = highPrev1_ = highPrev2_ = 0.0f;
}

void LinkwitzRileyCrossover::SetCutoff(float cutoff) {
    cutoff_ = cutoff;
    UpdateCoefficients();
}

void LinkwitzRileyCrossover::UpdateCoefficients() {
    float w0 = 2.0f * M_PI * cutoff_ / sampleRate_;
    float cosW0 = std::cos(w0);
    float sinW0 = std::sin(w0);

    float alpha = sinW0 / (2.0f * 0.707f); // Q = 0.707 for Butterworth response

    float norm = 1.0f / (1.0f + alpha);
    a0_ = (1.0f - cosW0) * 0.5f * norm;
    a1_ = (1.0f - cosW0) * norm;
    a2_ = a0_;
    b1_ = -2.0f * cosW0 * norm;
    b2_ = (1.0f - alpha) * norm;
}

void LinkwitzRileyCrossover::Process(float in, float& lowOut, float& highOut) {
    // Process low-pass filter twice (4th-order)
    float low = a0_ * in + a1_ * lowPrev1_ + a2_ * lowPrev2_ - b1_ * highPrev1_ - b2_ * highPrev2_;

    // Store previous values for LPF
    lowPrev2_ = lowPrev1_;
    lowPrev1_ = in;
    highPrev2_ = highPrev1_;
    highPrev1_ = low;

    // Apply LPF again for 4th order
    float lowFiltered = a0_ * low + a1_ * lowPrev1_ + a2_ * lowPrev2_ - b1_ * highPrev1_ - b2_ * highPrev2_;

    // High-pass output is calculated by subtracting the **4th order** lowpass from input
    float highFiltered = in - lowFiltered;

    // Assign outputs
    lowOut = lowFiltered;
    highOut = highFiltered;
}