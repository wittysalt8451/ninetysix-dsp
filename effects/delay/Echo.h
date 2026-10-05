#pragma once
#ifndef NINETYSIX_ECHO_H
#define NINETYSIX_ECHO_H

#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus

namespace ninetysix {

    /**
     * @brief Beat-synced stereo echo, modelled on the DJM Beat FX ECHO.
     *
     * The input repeats at the set delay (a beat fraction of the clock)
     * and fades out through the feedback. A low and a high cut in the loop
     * make every repeat a little thinner and darker, so a long tail never
     * piles up bass or fizz. The dry signal passes at unity; the amount
     * sets the echo level and how long the repeats last, like the DJM's
     * LEVEL/DEPTH.
     *
     * SetInput(false) stops feeding new audio but lets the repeats fade
     * out in tempo, the DJM "echo out" (cut the fader, keep the echo).
     * Delay changes crossfade between two read heads, so a tempo change
     * neither clicks nor bends the pitch.
     */
    class Echo {
        public:
            /** @brief Delay memory (e.g. SDRAM), one buffer per channel,
             *  zeroed here. Sets the longest delay. */
            void Init(float sample_rate, float* left, float* right, size_t size);

            /** @brief Delay in samples, e.g. one beat of the clock. */
            void SetDelaySamples(uint32_t samples);

            /** @brief 0..1: echo level and repeat length (LEVEL/DEPTH). */
            void SetAmount(float amount);

            /** @brief false: no new audio into the echo; the tail fades out. */
            void SetInput(bool on) { input_on_ = on; }

            /** @brief Dry plus echo, in place. */
            void Process(float& left, float& right);

            /** @brief Input still on, or repeats still audible (> -80 dB). */
            bool IsRinging() const;

        private:
            float Read(const float* buf, uint32_t delay) const;
            float Loop(float in, float& lp, float& hp) const;

            float*   buf_l_ = nullptr;
            float*   buf_r_ = nullptr;
            size_t   size_ = 0;
            size_t   write_pos_ = 0;

            // Delay, crossfaded between two read heads on a change
            uint32_t delay_ = 1;
            uint32_t old_delay_ = 1;
            uint32_t pending_delay_ = 1;
            uint32_t xfade_pos_ = 0;
            uint32_t xfade_len_ = 1;

            // Parameters with their per-sample smoothed values
            bool     input_on_ = false;
            float    amount_ = 0.0f;
            float    send_ = 0.0f;
            float    wet_ = 0.0f;
            float    feedback_ = 0.0f;
            float    smooth_ = 0.0f;

            // Loop filters: one-pole low cut and high cut per channel
            float    lp_coef_ = 0.0f;
            float    hp_coef_ = 0.0f;
            float    lp_l_ = 0.0f, hp_l_ = 0.0f;
            float    lp_r_ = 0.0f, hp_r_ = 0.0f;

            // Tail detection: loudest sample written in the current and the
            // previous pass through the delay
            float    pass_peak_ = 0.0f;
            float    last_pass_peak_ = 0.0f;
            uint32_t pass_pos_ = 0;
    };

} // namespace ninetysix

#endif
#endif
