#pragma once

namespace ninetysix {

/**
 * @brief One-pole DC blocking high-pass (HPF at ~20 Hz @ 48 kHz with default R).
 *
 * Typical use after asymmetric saturation or other DC-introducing waveshapes.
 */
class DcBlockOnePole {
public:
    /**
     * @param coefficient Feedback coefficient (0.9975f matches original PostDistortion).
     */
    void Init(float coefficient = 0.9975f);

    void Reset();

    /** @brief Process one sample; updates internal state. */
    float Process(float x);

private:
    float coefficient_ = 0.9975f;
    float x1_ = 0.f;
    float y1_ = 0.f;
};

} // namespace ninetysix
