#include "control/GatePulse.h"

namespace ninetysix {

void GatePulse::StartPulse(uint32_t nowMs) {
    active_  = true;
    startMs_ = nowMs;
}

void GatePulse::Update(uint32_t nowMs) {
    if (active_ && (nowMs - startMs_) >= kPulseDurationMs) {
        active_ = false;
    }
}

} // namespace ninetysix
