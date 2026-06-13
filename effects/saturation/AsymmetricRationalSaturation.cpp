#include "AsymmetricRationalSaturation.h"
#include <cmath>

namespace sudwalfulkaan {

void AsymmetricRationalSaturation::Init() {
    drive01_ = 0.f;
    kPos_ = kKMin;
    kNeg_ = kKMin;
}

void AsymmetricRationalSaturation::SetDrive(float drive01) {
    if (drive01 < 0.f) {
        drive01 = 0.f;
    }
    if (drive01 > 1.f) {
        drive01 = 1.f;
    }
    drive01_ = drive01;
    kPos_ = kKMin + drive01_ * (kKMaxPos - kKMin);
    kNeg_ = kKMin + drive01_ * (kKMaxNeg - kKMin);
}

float AsymmetricRationalSaturation::Process(float x) {
    if (drive01_ <= kDriveEps) {
        return x;
    }
    const float ax = std::fabs(x);
    const float k = (x >= 0.f) ? kPos_ : kNeg_;
    return x * (ax + k) / (x * x + (k - 1.f) * ax + 1.f);
}

} // namespace sudwalfulkaan
