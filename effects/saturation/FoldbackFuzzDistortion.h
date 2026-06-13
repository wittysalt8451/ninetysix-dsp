#pragma once

namespace sudwalfulkaan {

/**
 * @brief Foldback fuzz: pre-gain then triangle fold beyond ±1.
 *
 * Dense harmonics; gain at full drive is capped by an internal maximum
 * (tamer than a very wide fuzz range).
 */
class FoldbackFuzzDistortion {
public:
    void Init();

    /**
     * @brief Drive 0 = clean, 1 = heavy folding (gain = 1 + drive² · kMaxGain).
     */
    void SetDrive(float drive01);

    /** @brief Waveshape one sample (bypass when gain is near unity). */
    float Process(float x);

private:
    float drive01_ = 0.f;
    float fuzzGain_ = 1.f;
    static constexpr float kMaxGain = 4.5f;
};

} // namespace sudwalfulkaan
