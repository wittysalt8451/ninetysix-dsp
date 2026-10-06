#pragma once
#ifndef NINETYSIX_BITCRUSH_H
#define NINETYSIX_BITCRUSH_H

#include <math.h>

namespace ninetysix {

    /**
     * @brief Bitcrusher: rounds the signal to a lower bit depth.
     *
     * The depth may be fractional, so a swept depth crushes smoothly
     * instead of stepping from bit to bit. Full scale is +/-1: 16 bits
     * gives the 1/32768 steps of 16-bit audio. No state, so one instance
     * can serve both channels.
     */
    class Bitcrush {
        public:
            /** @brief Bit depth, 1..24 (fractional allowed); starts at 16. */
            void SetBits(float bits);

            float Process(float in) const {
                return roundf(in * scale_) * inv_scale_;
            }

            void Process(float& left, float& right) const {
                left = Process(left);
                right = Process(right);
            }

        private:
            float scale_ = 32768.0f;
            float inv_scale_ = 1.0f / 32768.0f;
    };

} // namespace ninetysix

#endif
