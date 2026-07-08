#pragma once

namespace ninetysix {

/**
 * @brief BPM estimation from a gate/clock input (Schmitt + interval timing).
 */
class ClockDetector
{
public:
    void Init(float sample_rate, float smoothing_factor = 0.1f);
    void Process(float gate_input, float current_time);
    float GetBPM() const;

private:
    float sample_rate_;
    float last_trigger_time_;
    float bpm_;
    bool gate_state_;
    float schmitt_high_threshold_;
    float schmitt_low_threshold_;
    float smoothed_bpm_;
    float smoothing_factor_;
};

} // namespace ninetysix
