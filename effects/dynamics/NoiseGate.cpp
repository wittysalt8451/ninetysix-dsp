#include "dynamics/NoiseGate.h"
#include <cmath>

using namespace ninetysix;

namespace {
    // One-pole smoothing coefficient for a time constant in ms
    float Coef(float ms, float sample_rate) {
        return 1.0f - std::exp(-1000.0f / (ms * sample_rate));
    }
}

void NoiseGate::Init(float sample_rate) {
    env_  = 0.0f;
    gain_ = 1.0f;
    env_attack_  = Coef(0.5f, sample_rate);   // track incoming level fast
    env_release_ = Coef(50.0f, sample_rate);  // ride over zero crossings
    open_rate_   = Coef(2.0f, sample_rate);   // any signal reopens instantly
    close_rate_  = Coef(300.0f, sample_rate); // floor fades out, no pumping
}

void NoiseGate::Process(float& left, float& right) {
    if (threshold_ <= 0.0f) return;

    const float al = left < 0.0f ? -left : left;
    const float ar = right < 0.0f ? -right : right;
    const float x = al > ar ? al : ar;
    env_ += (x - env_) * (x > env_ ? env_attack_ : env_release_);

    float target = 1.0f;
    if (env_ < threshold_) {
        const float t = env_ / threshold_;
        target = t * t;  // soft expansion, never a hard mute
    }
    gain_ += (target - gain_) * (target > gain_ ? open_rate_ : close_rate_);

    left *= gain_;
    right *= gain_;
}
