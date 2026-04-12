#include "Saturation.h"
#include <cmath>
#include "daisysp.h"

using namespace sudwalfulkaan;
using namespace daisysp;

void Saturation::Init(float gain = 1.0f, float drywet = 1.f, int mode = SOFT_CLIPPING) {
    gain_ = gain;
    mode_ = mode;
    drywet_ = drywet;
}
void Saturation::setDryWet(float drywet) {
    drywet_ = drywet;
}
void Saturation::SetGain(float gain) {
    gain_ = gain * 9.f + 1.f;
}

void Saturation::SetMode(float mode) {
    // Map mode to an evenly distributed integer value
    mode_ = static_cast<int>(mode);
}

int Saturation::GetMode() {
    return mode_;
}

float Saturation::Process(float in) {
    float x = in * gain_;
    float saturated = ApplySaturation(x);
    return drywet_ * saturated  + ((1.f - drywet_) * in);
}

float Saturation::ApplySaturation(float x) {
    switch (mode_) {        
        case SOFT_CLIPPING: return  x / (1 + std::fabs(x));
        case TANH_SATURATION: return std::tanh(x);
        case ARCTAN_SATURATION: return std::atan(x);
        case CUBIC_SATURATION: return x - (1.0f / 3.0f) * x * x * x;
        case QUADRATIC_SATURATION: return (x > 1.0f) ? (2.0f - (1.0f / x)) : x; 
        case SINE_SATURATION: return std::sin(x);
        case EXP_SATURATION: return 1 - exp(-std::fabs(x)) * (x < 0 ? -1 : 1); 
        case HARD_CLIP: return (std::max(-1.0f, std::min(1.0f, x)))*.8f; 
        case RECTIFIER_SATURATION: return std::fabs(x); 
        default: return x;
    }
}