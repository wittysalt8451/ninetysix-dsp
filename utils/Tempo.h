#pragma once

namespace ninetysix {

/**
 * @brief BPM-synced release time (seconds), clamped to [min_release, max_release].
 * @param division Larger = shorter time (e.g. 4 = quarter-note fraction of beat).
 */
float CalculateReleaseTime(float bpm,
                           float min_release = 0.01f,
                           float max_release = 1.0f,
                           float division    = 4.0f);

} // namespace ninetysix
