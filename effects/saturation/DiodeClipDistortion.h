#pragma once

namespace ninetysix {

/**
 * @brief Soft diode clip: symmetric, round onset.
 *
 * Transfer: \f$ y = x \frac{1+d}{1+|xd|} \f$ with \f$d\f$ mapped from a 0..1 drive
 * control. Memoryless; safe to run inside an oversampled path.
 */
class DiodeClipDistortion {
public:
    void Init();

    /**
     * @brief Drive 0 = clean, 1 = strong saturation (mapped internally).
     */
    void SetDrive(float drive01);

    /** @brief Waveshape one sample (bypass when drive is near zero). */
    float Process(float x);

private:
    float drive01_ = 0.f;
    float diodeDrive_ = 0.f;
    static constexpr float kMaxDrive = 12.f;
    static constexpr float kDriveEps = 0.05f;
};

} // namespace ninetysix
