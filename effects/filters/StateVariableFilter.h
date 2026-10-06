#pragma once
#ifndef NINETYSIX_STATE_VARIABLE_FILTER_H
#define NINETYSIX_STATE_VARIABLE_FILTER_H

namespace ninetysix {

    /**
     * @brief Double-sampled, stable state variable filter: low, high, band,
     * notch and peak outputs from one pass.
     *
     * Port of DaisySP's Svf (Andrew Simper's "State Variable Filter (Double
     * Sampled, Stable)" from musicdsp.org; DaisySP is MIT licensed,
     * (c) 2020 Electrosmith). Matches it sample for sample, so swapping it
     * in does not change the sound:
     *
     * - The cutoff tops out at a third of the sample rate.
     * - The drive soft-limits the resonance, scaled by the resonance as in
     *   DaisySP (0.5 by default, so 0.2 at a resonance of 0.4).
     */
    class StateVariableFilter {
        public:
            void Init(float sample_rate);

            /** @brief Filter one sample; updates all outputs. */
            void Process(float in);

            /** @brief Cutoff in Hz, up to sample_rate / 3. */
            void SetFreq(float hz);

            /** @brief Resonance 0..1 (1 = self-oscillation edge). */
            void SetRes(float res);

            /** @brief Drive 0..10 (DaisySP scale): how hard the resonance
             *  is soft-limited. */
            void SetDrive(float drive);

            float Low() const { return out_low_; }
            float High() const { return out_high_; }
            float Band() const { return out_band_; }
            float Notch() const { return out_notch_; }
            float Peak() const { return out_peak_; }

        private:
            void UpdateDamp();

            float sample_rate_ = 48000.0f;
            float fc_max_ = 16000.0f;
            float res_ = 0.5f;
            float drive_ = 0.5f;
            float pre_drive_ = 0.5f;
            float freq_ = 0.25f;
            float damp_ = 0.0f;
            float low_ = 0.0f, band_ = 0.0f;
            float out_low_ = 0.0f, out_high_ = 0.0f, out_band_ = 0.0f;
            float out_notch_ = 0.0f, out_peak_ = 0.0f;
    };

} // namespace ninetysix

#endif
