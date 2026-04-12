#include "Tanh.h"
#include <cmath>

using namespace sudwalfulkaan;

void TanhSaturation::Init()
{
    gain_ = 1.0f;       // Default gain
    asymmetry_ = 0.0f;  // Default symmetric distortion
}

float TanhSaturation::Process(float in)
{
    // Apply input gain
    float x = in * gain_;

    // Introduce asymmetry by shifting the signal before tanh
    x += asymmetry_ * 0.5f;  // Small offset for asymmetry effect

    // Apply tanh saturation
    return std::tanh(x);
}
