#pragma once

#include <cmath>

namespace ninetysix {

/**
 * @brief Minimal phase-accumulator LFO, drop-in for the daisysp::Oscillator
 * usage in the modulation effects.
 *
 * Matches the DaisySP conventions exactly so swapping it in does not change
 * the sound: output is computed from the current phase and the phase advances
 * afterwards, waveforms span -1..1 (DaisySP applied a default amplitude of
 * 0.5 on top — scale at the call site to keep behaviour identical).
 */
class Lfo {
public:
    void Init(float sample_rate) {
        sample_rate_ = sample_rate > 0.0f ? sample_rate : 48000.0f;
        phase_ = 0.0f;
        inc_ = 0.0f;
    }

    void SetFreq(float freq) { inc_ = freq / sample_rate_; }

    /** @param phase Normalized 0..1 */
    void SetPhase(float phase) { phase_ = phase - floorf(phase); }

    /** sin(2*pi*phase), -1..1 */
    float ProcessSine() {
        const float out = sinf(phase_ * kTwoPi);
        Advance();
        return out;
    }

    /** Triangle starting at +1 (DaisySP WAVE_TRI shape), -1..1 */
    float ProcessTriangle() {
        const float t = -1.0f + 2.0f * phase_;
        const float out = 2.0f * (fabsf(t) - 0.5f);
        Advance();
        return out;
    }

private:
    static constexpr float kTwoPi = 6.28318530717958647692f;

    void Advance() {
        phase_ += inc_;
        if (phase_ >= 1.0f) phase_ -= 1.0f;
        if (phase_ < 0.0f) phase_ += 1.0f;
    }

    float sample_rate_ = 48000.0f;
    float phase_ = 0.0f;
    float inc_ = 0.0f;
};

} // namespace ninetysix
