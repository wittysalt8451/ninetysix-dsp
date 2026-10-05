#pragma once
#ifndef NINETYSIX_RESONATOR_H
#define NINETYSIX_RESONATOR_H

#include "utils/DelayLine.h"

namespace ninetysix {

    /**
     * @brief Stereo comb resonator: a tuned, damped feedback delay that
     * rings at a pitch and its harmonics.
     *
     * DJM mixers have no effect by this name; DJs get the sound from
     * SPIRAL with its time set to ~10 ms (a short delay with feedback).
     * Here the pitch is set directly and glides when changed, as SPIRAL's
     * does when its time moves.
     *
     * - The ring time is the same at every pitch: the feedback follows the
     *   delay length (Karplus-Strong style), up to a stability limit.
     * - A high cut in the loop makes the upper harmonics die first, so the
     *   ring sounds like a string rather than a metallic buzz.
     * - Input below ~80 Hz is kept out, so kicks don't set off a boom.
     * - The wet is scaled to about equal loudness on music-like material at
     *   any pitch and feedback; it is added on top of the dry.
     * - The right channel sits a few cents sharp for width.
     */
    class Resonator {
        public:
            /** @brief Starts on pitch and at the set mix. */
            void Init(float sample_rate);

            /** @brief Silence the ring, jump to the set pitch and fade the
             *  wet back in (call when the effect is switched back on). */
            void Reset();

            /** @brief Fundamental in Hz (20..4000); glides to it. */
            void SetFreq(float hz);

            /** @brief Time for the ring to fall 60 dB, in seconds. */
            void SetRingTime(float seconds);

            /** @brief Wet level added to the dry, 0..1. */
            void SetMix(float mix);

            void Process(float& left, float& right);

        private:
            static constexpr size_t kMaxDelay = 2600; // 20 Hz at 48 kHz

            float Channel(float in, float delay, float feedback, float norm,
                          DelayLine<kMaxDelay>& line, float& lo, float& lp);

            DelayLine<kMaxDelay> line_l_, line_r_;
            float sample_rate_ = 48000.0f;
            float target_freq_ = 220.0f;
            float freq_ = 220.0f;
            float ring_time_ = 0.6f;
            float mix_ = 0.0f;
            float wet_ = 0.0f;
            float smooth_ = 0.0f;
            float glide_ = 0.0f;
            float in_cut_coef_ = 0.0f;
            float damp_coef_ = 0.0f;
            float lo_l_ = 0.0f, lo_r_ = 0.0f; // input low cut state
            float lp_l_ = 0.0f, lp_r_ = 0.0f; // loop high cut state
    };

} // namespace ninetysix

#endif
