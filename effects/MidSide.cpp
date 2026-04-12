#include "MidSide.h"
#include <cmath>

using namespace sudwalfulkaan;

void MidSide::Init(float width, float sampleRate) {
    sampleRate_ = sampleRate;
    width_ = width;
}

void MidSide::Process(float& left, float& right)
{
    float mid = (left + right) * 0.5f;
    float side = (left - right) * 0.5f;

    left = mid + width_ * side;
    right = mid - width_ * side;
}

void MidSide::SetWidth(float width) {
    width_ = width * 2;
}
