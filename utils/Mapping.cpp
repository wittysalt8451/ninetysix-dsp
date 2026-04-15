#include "utils/Mapping.h"
#include <algorithm>
#include <cmath>

namespace sudwalfulkaan {

namespace {
float Clamp01(float x) {
    return std::max(0.0f, std::min(1.0f, x));
}
} // namespace

float MapLinear(float normalized, float min, float max) {
    float t = Clamp01(normalized);
    return min + t * (max - min);
}

float MapLogarithmic(float normalized, float min, float max) {
    float t = Clamp01(normalized);
    min = std::max(min, 1e-12f);
    max = std::max(max, 1e-12f);
    float logMin = std::log(min);
    float logMax = std::log(max);
    return std::exp(logMin + t * (logMax - logMin));
}

} // namespace sudwalfulkaan
