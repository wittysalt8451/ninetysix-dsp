#pragma once

namespace sudwalfulkaan {

/**
 * @brief Asymmetric rational variable-gain saturator.
 *
 * Positive and negative halves use different curve sharpness (k_pos vs k_neg)
 * derived from a single 0..1 drive control, emphasizing even harmonics at
 * moderate levels. Memoryless.
 */
class AsymmetricRationalSaturation {
public:
    void Init();

    /**
     * @brief Drive 0 = symmetric / clean (k_pos = k_neg = 1), 1 = strong asymmetry.
     */
    void SetDrive(float drive01);

    /** @brief Waveshape one sample (bypass when drive is near zero). */
    float Process(float x);

private:
    float drive01_ = 0.f;
    float kPos_ = 1.f;
    float kNeg_ = 1.f;
    static constexpr float kKMin = 1.f;
    static constexpr float kKMaxPos = 10.f;
    static constexpr float kKMaxNeg = 1.5f;
    static constexpr float kDriveEps = 0.02f;
};

} // namespace sudwalfulkaan
