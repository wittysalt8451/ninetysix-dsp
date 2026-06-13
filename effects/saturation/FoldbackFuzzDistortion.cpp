#include "FoldbackFuzzDistortion.h"
#include <cmath>

namespace sudwalfulkaan {

void FoldbackFuzzDistortion::Init() {
    drive01_ = 0.f;
    fuzzGain_ = 1.f;
}

void FoldbackFuzzDistortion::SetDrive(float drive01) {
    if (drive01 < 0.f) {
        drive01 = 0.f;
    }
    if (drive01 > 1.f) {
        drive01 = 1.f;
    }
    drive01_ = drive01;
    const float d2 = drive01_ * drive01_;
    fuzzGain_ = 1.f + d2 * kMaxGain;
}

float FoldbackFuzzDistortion::Process(float x) {
    if (fuzzGain_ <= 1.01f) {
        return x;
    }
    const float driven = x * fuzzGain_;
    float m = std::fmod(driven - 1.f, 4.f);
    if (m < 0.f) {
        m += 4.f;
    }
    return std::fabs(m - 2.f) - 1.f;
}

} // namespace sudwalfulkaan
