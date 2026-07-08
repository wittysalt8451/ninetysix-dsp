#include "utils/Tempo.h"
#include <algorithm>

namespace ninetysix {

float CalculateReleaseTime(float bpm, float min_release, float max_release, float division) {
    if (bpm <= 0.0f) {
        return min_release;
    }
    float beat_time   = 60.0f / bpm;
    float release_time = beat_time / division;
    return std::max(min_release, std::min(release_time, max_release));
}

} // namespace ninetysix
