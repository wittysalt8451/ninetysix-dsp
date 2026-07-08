#pragma once

#include <cstdint>

namespace ninetysix {

/**
 * @brief Generates timed gate pulses for CV sequencer gate outputs.
 *
 * Used for two purposes:
 *   1. Note gate dip: briefly pulls the note gate LOW for 2ms on each new step,
 *      so a downstream envelope can re-trigger even on repeated pitches.
 *   2. Root gate trigger: briefly drives the root gate HIGH for 2ms when the
 *      Markov chain returns to the root note.
 *
 * Usage:
 *   - Call StartPulse() when the event fires.
 *   - Call Update(nowMs) every loop iteration.
 *   - Read IsPulseActive() to drive the gate output.
 *
 * The caller decides whether "active" means high or low on the physical pin.
 */
class GatePulse {
public:
    static constexpr uint32_t kPulseDurationMs = 2;

    /** @brief Starts a new pulse. Resets the timer if already active. */
    void StartPulse(uint32_t nowMs);

    /**
     * @brief Updates the pulse timer. Call every loop iteration.
     * @param nowMs Current time from System::GetNow().
     */
    void Update(uint32_t nowMs);

    /**
     * @brief Returns true while the pulse is active (within kPulseDurationMs).
     */
    bool IsPulseActive() const { return active_; }

private:
    bool     active_{false};
    uint32_t startMs_{0};
};

} // namespace ninetysix
