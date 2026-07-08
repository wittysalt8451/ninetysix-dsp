#pragma once

namespace ninetysix {

/**
 * @brief One-pole smoothed level follower for stereo audio (mono level, 0..1).
 *
 * Replaces the old file-level globals in tools.cpp: each instance has its own state.
 */
class EnvelopeFollower {
public:
    EnvelopeFollower() = default;

    /** @param smoothing 0..1, higher = smoother (typ. 0.95–0.99). */
    void Init(float smoothing = 0.99f);

    void SetSmoothing(float smoothing);

    /** Stereo input; returns smoothed level in [0, 1]. */
    float Process(float inL, float inR);

private:
    float smooth_cv_      = 0.0f;
    float smoothing_      = 0.99f;
};

} // namespace ninetysix
