#include "Ducker.h"

namespace sudwalfulkaan
{
    void Ducker::Init(float ducking_amount, float ducking_release, float sample_rate)
    {
        ducking_amount_ = ducking_amount;
        ducking_release_ = ducking_release;
        sample_rate_ = sample_rate;
        current_duck_level_ = 1.0f;
    }

    void Ducker::Update(bool gate_state)
    {
        if (gate_state)
            current_duck_level_ = ducking_amount_; // Instant ducking when gate is high
        else
            current_duck_level_ += (1.0f - current_duck_level_) * (1.0f / (ducking_release_ * sample_rate_)); // Smooth restore
    }

    void Ducker::SetRelease(float release)
    {
        ducking_release_ = release;
    }

    void Ducker::Process(float &left_sample, float &right_sample)
    {
        left_sample *= current_duck_level_;
        right_sample *= current_duck_level_;
    }
}
