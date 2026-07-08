#include "dynamics/Limiter.h"
#include <cmath>
#include "daisysp.h"

// Both daisysp and ninetysix define a Limiter, so definitions must be
// explicitly qualified
using namespace ninetysix;

void ninetysix::Limiter::Init(float threshold, float attack, float release) {
    threshold_ = threshold;
    attack_ = attack;
    release_ = release;
    gain_ = 1.0f;
    envelope_ = 0.0f;
}

float ninetysix::Limiter::Process(float in) {
    float abs_in = std::fabs(in);

    // Envelope follower with attack & release
    if (abs_in > envelope_) {
        envelope_ = envelope_ + (abs_in - envelope_) * attack_; // Faster attack
    } else {
        envelope_ = envelope_ + (abs_in - envelope_) * release_; // Slower release
    }

    // Compute gain reduction factor
    float gain_reduction = (envelope_ > threshold_) ? threshold_ / envelope_ : 1.0f;
    gain_ = gain_reduction;

    // Apply gain reduction
    float out = in * gain_;

    // Soft clipping with a smooth transition
    if (out > threshold_) {
        out = threshold_ + (out - threshold_) / (1.0f + std::exp((out - threshold_) * softness_));
    } else if (out < -threshold_) {
        out = -threshold_ - (out + threshold_) / (1.0f + std::exp(-(out + threshold_) * softness_));
    }

    return out;
}

void ninetysix::Limiter::SetThreshold(float t) { threshold_ = t; }
void ninetysix::Limiter::SetAttack(float a) { attack_ = a; }
void ninetysix::Limiter::SetRelease(float r) { release_ = r; }
void ninetysix::Limiter::SetSoftness(float s) { softness_ = s; }
void ninetysix::Limiter::SetEnvelope(float e) {
    // Clamp input range (0.0 - 1.0)
    e = daisysp::fclamp(e, 0.0f, 1.0f);

    // Map attack: 0.001s (1ms) → 0.1s (100ms) (logarithmic feel)
    float attack = 0.1f - std::log(e + 1.0f) * 0.099f;

    // Map release: 0.01s (10ms) → 1.0s (1000ms) (longer range)
    float release = 0.01f + std::pow(e, 1.5f) * 0.99f;

    // Apply to limiter
    SetAttack(attack);
    SetRelease(release);
}
