#pragma once

namespace sudwalfulkaan
{
    class Ducker
    {
    public:
        /**
         * @brief Initializes the stereo ducker.
         * @param duck_amount How much to reduce volume when gate is active (0.0 - 1.0).
         * @param release_time Time in seconds to restore volume when gate is released.
         * @param sample_rate Audio sample rate (needed for smoothing calculation).
         */
        void Init(float duck_amount = 0.5f, float release_time = 0.2f, float sample_rate = 48000.0f);

        /**
         * @brief Updates the ducking state based on the gate input.
         * @param gate_state Boolean gate signal (true = ducking active).
         */
        void Update(bool gate_state);

        /**
         * @brief Applies the ducking effect to stereo samples.
         * @param left_sample The input sample for the left channel.
         * @param right_sample The input sample for the right channel.
         */
        void Process(float &left_sample, float &right_sample);

        /**
         * @brief Sets the release time for ducking recovery.
         * @param release Release time in seconds.
         */
        void SetRelease(float release);

    private:
        float ducking_amount_;
        float ducking_release_;
        float sample_rate_;
        float current_duck_level_;
    };
}
