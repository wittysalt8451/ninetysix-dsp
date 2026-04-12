#include "ClockDetector.h"

using namespace sudwalfulkaan;

void ClockDetector::Init(float sample_rate, float smoothing_factor = 0.1f)
{
    sample_rate_ = sample_rate;
    last_trigger_time_ = -1.0f;
    bpm_ = 0.0f;
    gate_state_ = false;
    schmitt_high_threshold_ = 0.7f;
    schmitt_low_threshold_ = 0.3f;
    smoothed_bpm_ = 0.0f;
    smoothing_factor_ = smoothing_factor;
}

void ClockDetector::Process(float gate_input, float current_time)
{
    bool new_state = gate_state_;
    if (gate_input > schmitt_high_threshold_)
    {
        new_state = true;
    }
    else if (gate_input < schmitt_low_threshold_)
    {
        new_state = false;
    }

    if (!gate_state_ && new_state)
    {
        if (last_trigger_time_ > 0.0f)
        {
            float interval = (current_time - last_trigger_time_) / 1000.0f; // Convert ms to seconds
            bpm_ = 60.0f / interval;
            smoothed_bpm_ = (smoothing_factor_ * bpm_) + ((1.0f - smoothing_factor_) * smoothed_bpm_);
        }
        last_trigger_time_ = current_time;
    }
    gate_state_ = new_state;
}

float ClockDetector::GetBPM() const { return smoothed_bpm_; }
