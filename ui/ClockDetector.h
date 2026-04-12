#ifndef CLOCK_DETECTOR_H
#define CLOCK_DETECTOR_H

namespace sudwalfulkaan {

    class ClockDetector
    {
    public:
        void Init(float sample_rate, float smoothing_factor);
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
}

#endif // CLOCK_DETECTOR_H
