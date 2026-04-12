#include <math.h>

namespace sudwalfulkaan
{
    // Envelope follower variables
    float smooth_cv = 0.0f;
    float smoothing_factor = 0.99f;  // Higher = smoother (0.95–0.99 recommended)

    float logMapping(float potValue, float min, float max) {
        // Ensure the potValue is within the expected range
        potValue = std::max(0.0f, std::min(1.0f, potValue));

        // Logarithmic mapping
        float logMin = std::log(min);
        float logMax = std::log(max);
        return std::exp(logMin + potValue * (logMax - logMin));
    }

    float mapping(float potValue, float min, float max) {
        // Ensure the potValue is within the expected range
        potValue = std::max(0.0f, std::min(1.0f, potValue));

        return min + potValue * (max - min);
    }


    float envelopeFollower(float inL, float inR) {
        // Get stereo input and convert to mono level
        float level = (fabsf(inL) + fabsf(inR)) * 0.5f; // Averaged stereo level

        // Apply smoothing (1-pole low-pass filter for fluid CV response)
        smooth_cv = (smoothing_factor * smooth_cv) + ((1.0f - smoothing_factor) * level);
        return std::max(0.f, std::min(1.f, smooth_cv));
    }

    // Slew Limiter function (One-Pole Low-Pass Filter)
    float SlewLimiter(float target, float current, float slewRate) {
        return current + slewRate * (target - current);
    }

    /**
    * @brief Calculates a BPM-synced release time for a limiter.
    *
    * This function determines the release time based on the current BPM and a musical division.
    * The release time is clamped within a given range to prevent extreme values.
    *
    * @param bpm The beats per minute (BPM) of the current tempo.
    * @param min_release The minimum release time in seconds (default: 0.01s or 10ms).
    * @param max_release The maximum release time in seconds (default: 1.0s or 1000ms).
    * @param division The beat subdivision factor (default: 4.0, corresponding to a sixteenth note).
    *                 - 1.0 = whole note
    *                 - 2.0 = half note
    *                 - 4.0 = quarter note
    *                 - 8.0 = eighth note, etc.
    *
    * @return The calculated release time in seconds, clamped within the given range.
    */
    float CalculateReleaseTime(float bpm, float min_release = 0.01f, float max_release = 1.0f, float division = 4.0f)
    {
        if (bpm <= 0)
            return min_release; // Prevent division by zero

        float beat_time = 60.0f / bpm; // Quarter note duration in seconds
        float release_time = beat_time / division; // Sync release time to a subdivision

        // Clamp release time to valid range (0.01s to 1s)
        return fmaxf(min_release, fminf(release_time, max_release));
    }

    // void MakeMonoBelowFreq(float* left, float* right, float cutoffFreq, size_t bufferSize, float sampleRate) {
    //     static float lowPassLeft = 0.0f;
    //     static float lowPassRight = 0.0f;
    //     float alpha = 2.0f * M_PI * cutoffFreq / sampleRate; // Filter coefficient

    //     for (size_t i = 0; i < bufferSize; i++) {
    //         // Low-pass filter for left and right channels
    //         lowPassLeft += alpha * (left[i] - lowPassLeft);
    //         lowPassRight += alpha * (right[i] - lowPassRight);

    //         // Combine the low frequencies to make them mono
    //         float monoLow = (lowPassLeft + lowPassRight) * 0.5f;

    //         // Replace the low frequencies in both channels
    //         left[i] = monoLow + (left[i] - lowPassLeft);   // High frequencies remain stereo
    //         right[i] = monoLow + (right[i] - lowPassRight);
    //     }
    // }
}