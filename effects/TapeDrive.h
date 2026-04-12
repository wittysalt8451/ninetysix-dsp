#pragma once
#ifndef SWF_TAPE_DRIVE_H
#define SWF_TAPE_DRIVE_H

#include <cmath>

namespace sudwalfulkaan {

/**
 * TapeDrive - Analog tape saturation emulation with low-pass filter
 * 
 * Combines soft saturation (tape-style warmth) with a one-pole low-pass filter
 * to emulate the frequency response of analog tape machines.
 */
class TapeDrive {
public:
    TapeDrive() {}
    ~TapeDrive() {}

    /**
     * Initialize the TapeDrive effect
     * @param sample_rate Audio sample rate in Hz
     */
    void Init(float sample_rate);

    /**
     * Process a single sample through the tape drive
     * @param in Input sample
     * @return Processed sample with saturation and filtering
     */
    float Process(float in);

    /**
     * Set the drive amount (saturation intensity)
     * @param drive Drive amount (1.0 = clean, higher = more saturation)
     *              Recommended range: 1.0 - 10.0
     */
    void SetDrive(float drive);

    /**
     * Set the low-pass filter cutoff frequency
     * @param freq Cutoff frequency in Hz (20 - 20000)
     */
    void SetFilterFreq(float freq);

    /**
     * Set dry/wet mix
     * @param mix Mix amount (0.0 = dry, 1.0 = wet)
     */
    void SetMix(float mix);

    /**
     * Enable/disable the low-pass filter
     * @param enabled True to enable filter
     */
    void SetFilterEnabled(bool enabled);

private:
    float sample_rate_;
    float drive_;           // Saturation amount (1.0 - 10.0)
    float filter_freq_;     // Low-pass cutoff frequency
    float mix_;             // Dry/wet mix (0.0 - 1.0)
    bool filter_enabled_;   // Filter bypass flag
    
    // One-pole low-pass filter state
    float lp_state_;
    float lp_coeff_;
    
    // Update filter coefficient based on frequency
    void UpdateFilterCoeff();
    
    // Soft saturation function (tape-style)
    float Saturate(float in);
};

} // namespace sudwalfulkaan

#endif // SWF_TAPE_DRIVE_H

