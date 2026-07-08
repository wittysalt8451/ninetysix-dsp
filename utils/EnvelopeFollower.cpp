#include "utils/EnvelopeFollower.h"
#include <cmath>
#include <algorithm>

namespace ninetysix {

void EnvelopeFollower::Init(float smoothing) {
    smooth_cv_ = 0.0f;
    smoothing_ = std::max(0.0f, std::min(1.0f, smoothing));
}

void EnvelopeFollower::SetSmoothing(float smoothing) {
    smoothing_ = std::max(0.0f, std::min(1.0f, smoothing));
}

float EnvelopeFollower::Process(float inL, float inR) {
    float level = (std::fabs(inL) + std::fabs(inR)) * 0.5f;
    smooth_cv_ = smoothing_ * smooth_cv_ + (1.0f - smoothing_) * level;
    return std::max(0.0f, std::min(1.0f, smooth_cv_));
}

} // namespace ninetysix
