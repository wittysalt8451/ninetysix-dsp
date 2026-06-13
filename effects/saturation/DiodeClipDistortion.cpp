#include "DiodeClipDistortion.h"
#include <cmath>

namespace sudwalfulkaan {

void DiodeClipDistortion::Init() {
    drive01_ = 0.f;
    diodeDrive_ = 0.f;
}

void DiodeClipDistortion::SetDrive(float drive01) {
    if (drive01 < 0.f) {
        drive01 = 0.f;
    }
    if (drive01 > 1.f) {
        drive01 = 1.f;
    }
    drive01_ = drive01;
    const float d2 = drive01_ * drive01_;
    diodeDrive_ = d2 * kMaxDrive;
}

float DiodeClipDistortion::Process(float x) {
    if (diodeDrive_ <= kDriveEps) {
        return x;
    }
    return x * (1.f + diodeDrive_) / (1.f + std::fabs(x * diodeDrive_));
}

} // namespace sudwalfulkaan
