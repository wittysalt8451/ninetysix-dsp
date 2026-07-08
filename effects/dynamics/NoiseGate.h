#pragma once
#ifndef NINETYSIX_NOISEGATE_H
#define NINETYSIX_NOISEGATE_H

#ifdef __cplusplus

namespace ninetysix {

    /**
     * @brief Soft stereo noise gate (downward expander).
     *
     * Meant to sit at the input of a high-gain chain: fully transparent
     * above the threshold (gain exactly 1), below it the gain falls off
     * softly (squared curve) instead of hard-muting. Opens fast, closes
     * slowly, so transients pass unharmed and the floor fades out without
     * pumping. A threshold of 0 disables it entirely.
     */
    class NoiseGate {
        public:
            void Init(float sample_rate);

            /** @brief Linear amplitude threshold (e.g. 0.0005 = -66 dBFS). 0 disables. */
            void SetThreshold(float threshold) { threshold_ = threshold; }

            /** @brief In-place stereo processing; both channels share one gain. */
            void Process(float& left, float& right);

        private:
            float threshold_   = 0.0005f;
            float env_         = 0.0f;
            float gain_        = 1.0f;
            float env_attack_  = 0.1f;
            float env_release_ = 0.001f;
            float open_rate_   = 0.01f;
            float close_rate_  = 0.0001f;
    };

} // namespace ninetysix

#endif
#endif
