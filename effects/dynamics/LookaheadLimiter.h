#pragma once
#ifndef NINETYSIX_LOOKAHEADLIMITER_H
#define NINETYSIX_LOOKAHEADLIMITER_H

#include <stdint.h>

namespace ninetysix {

    /**
     * @brief Clean stereo brickwall limiter with lookahead.
     *
     * The audio is delayed by the lookahead, so the gain has already glided
     * down by the time a peak comes out: no sample leaves above the ceiling,
     * and nothing is clipped or waveshaped on the way. The drive pushes the
     * signal into the ceiling (1 = only catch what would go over it). Both
     * channels share one gain, so the stereo image holds. The gain falls
     * over the lookahead and recovers with the release.
     */
    class LookaheadLimiter {
        public:
            /** @brief Longest lookahead in samples (~5 ms at 48 kHz). */
            static constexpr int kMaxLookahead = 256;

            /**
             * @param lookahead_ms Attack time, and the latency it adds
             * @param release_ms Recovery time constant
             */
            void Init(float sample_rate, float lookahead_ms = 1.0f, float release_ms = 150.0f);

            /** @brief Highest output level, linear (e.g. 0.9); starts at 1. */
            void SetCeiling(float ceiling) { ceiling_ = ceiling; }

            /** @brief Gain into the ceiling, linear (1 = none); starts at 1. */
            void SetDrive(float drive) { drive_ = drive; }

            /** @brief In-place stereo processing; the output lags by Latency(). */
            void Process(float& left, float& right);

            /** @brief Delay the limiter adds, in samples. */
            int Latency() const { return length_ - 1; }

        private:
            int   length_    = 1;     // lookahead window in samples
            float inv_length_ = 1.0f;
            float ceiling_   = 1.0f;
            float drive_     = 1.0f;
            float release_   = 0.0f;  // one-pole recovery coefficient
            float released_  = 1.0f;  // held gain after the release
            float box_sum_   = 1.0f;  // sum of box_, smooths the attack
            int   pos_       = 0;

            float delay_l_[kMaxLookahead];
            float delay_r_[kMaxLookahead];
            float box_[kMaxLookahead];

            // Sliding minimum of the gain each sample needs, as a monotonic
            // queue: gains that can never be the lowest again are dropped
            float    min_gain_[kMaxLookahead];
            uint32_t min_time_[kMaxLookahead];
            int      min_head_  = 0;
            int      min_count_ = 0;
            uint32_t now_       = 0;
    };

} // namespace ninetysix

#endif
