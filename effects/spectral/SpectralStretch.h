#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "utils/Fft.h"
#include "utils/SpscRingBuffer.h"

namespace ninetysix {

/**
 * @brief Caller-owned storage for one SpectralStretch channel.
 *
 * All pointers must remain valid for the lifetime of the engine. On embedded
 * targets place every buffer in SDRAM. Sizes are in floats.
 */
struct SpectralStretchBuffers {
    float* record = nullptr;       ///< Input history ring, `recordSize` floats.
    size_t recordSize = 0;         ///< Power of two, >= 4 * maxWindowSize.
    float* grainReal = nullptr;    ///< FFT workspace, `maxWindowSize` floats.
    float* grainImag = nullptr;    ///< FFT workspace, `maxWindowSize` floats.
    float* window = nullptr;       ///< Analysis window table, `maxWindowSize` floats.
    float* prevGrain = nullptr;    ///< Previous grain half, `maxWindowSize / 2` floats.
    float* fadeOld = nullptr;      ///< Crossfade-out table, `maxWindowSize / 2` floats.
    float* fadeNew = nullptr;      ///< Crossfade-in table, `maxWindowSize / 2` floats.
    float* magnitude = nullptr;    ///< Half spectrum, `maxWindowSize / 2 + 1` floats.
    float* noiseFloor = nullptr;   ///< Per-bin floor tracker, `maxWindowSize / 2 + 1` floats.
    float* twiddleCos = nullptr;   ///< FFT twiddles, `maxWindowSize / 2` floats.
    float* twiddleSin = nullptr;   ///< FFT twiddles, `maxWindowSize / 2` floats.
    float* outputRing = nullptr;   ///< Synthesized audio FIFO, `outputRingSize` floats.
    size_t outputRingSize = 0;     ///< Power of two, >= maxWindowSize.
    size_t maxWindowSize = 0;      ///< Largest supported FFT size, power of two.
};

/**
 * @brief Mono paulstretch-style spectral time stretcher.
 *
 * Continuously records input into a history ring while a slow playhead trails
 * the record head. Grains taken at the playhead are windowed, transformed,
 * given fully randomized phases (optionally with a spectral pitch shift) and
 * transformed back. Magnitudes are preserved, time structure is dissolved.
 *
 * Synthesis follows paulstretch (Nasca Octavian Paul): grains are taken one
 * hop (= windowSize / 2) apart and, because their phases are fully random,
 * come out of the inverse FFT with a statistically flat envelope. Each output
 * hop is a raised-cosine crossfade between the new grain and the previous
 * one, multiplied by a fixed equal-power compensation curve. Only two grains
 * ever sound at once, which is what gives paulstretch its smooth, pad-like
 * character (a windowed overlap-add of random-phase grains flutters at the
 * hop rate instead).
 *
 * Live behaviour:
 *  - The playhead crawls at hop / stretch, so the lag behind real time grows
 *    while stretching; the playhead resyncs near the record head shortly
 *    before its grain would be overwritten.
 *  - A time-domain onset detector at the record head snaps the playhead
 *    forward on transients, but only once the playhead has drifted more than
 *    a stretch-scaled distance behind (windowSize + stretch * hop). At low
 *    stretch the output therefore stays tight on the input; at high stretch
 *    the wash blooms for seconds before the next hit re-anchors it, keeping
 *    the stretch factor audible. TriggerCatchUp() snaps unconditionally.
 *    The snap lands the transient at the very end of the analysis window
 *    (the newest fully recorded grain); a short advance credit then makes
 *    the playhead ride the edge of the recorded data, so the transient
 *    sweeps to the window center — full level — half a window after the
 *    hit, regardless of the stretch factor, and lingers there.
 *  - Bins below a fixed spectral gate are muted before resynthesis: phase
 *    randomization would otherwise turn faint steady tones (codec hiss,
 *    supply whine) into a clearly audible constant beep in the wash.
 *  - On top of the fixed gate, a per-bin noise-floor tracker learns any
 *    stationary spectral component (rising slowly, falling quickly, capped
 *    so real signals above the cap are never affected) and mutes bins that
 *    do not move above their own floor. A constant whine therefore fades
 *    out of the wash within seconds regardless of its exact level, while
 *    changing musical content always passes.
 *
 * Several table buffers (window, fadeOld, fadeNew, magnitude, twiddles) may
 * be shared between engine instances to save fast RAM, provided the engines
 * always use the same window size and their Update() calls come from the
 * same thread. Record, grain, prevGrain and output buffers are per instance.
 *
 * Threading model (single producer / single consumer per side):
 *  - WriteInput() and ReadOutput() are real-time safe; call them from the
 *    audio callback.
 *  - Update(), SetWindowSize(), SetStretchFactor(), SetPitchRatio() and
 *    SetFreeze() must be called from one non-interrupt context (main loop).
 *  - TriggerCatchUp() may be called from any context.
 */
class SpectralStretch {
public:
    /**
     * @brief Bind buffers and reset all state. Not real-time safe.
     * @param sampleRate Audio sample rate in Hz.
     * @param buffers Caller-owned storage; validated for size and alignment.
     * @param rngSeed Non-zero seed; use distinct seeds per channel so the
     *        randomized phases decorrelate between left and right.
     * @return true when the buffer set is complete and consistent.
     */
    bool Init(float sampleRate, const SpectralStretchBuffers& buffers,
              uint32_t rngSeed);

    /** @brief Audio-callback side: record input. Ignored while frozen. */
    void WriteInput(const float* input, size_t count);

    /**
     * @brief Audio-callback side: pop synthesized audio.
     *
     * Fills `output` completely; zero-pads on underrun (e.g. right after
     * boot or a window size change) and counts the missing samples.
     */
    void ReadOutput(float* output, size_t count);

    /** @brief Main-loop side: synthesize grains until the FIFO is topped up. */
    void Update();

    /**
     * @brief Main-loop side: change the analysis window (FFT) size.
     * @param size Power of two in [kMinWindowSize, buffers.maxWindowSize].
     * @return true when the size was accepted (or already active).
     */
    bool SetWindowSize(size_t size);

    /** @brief How slowly the playhead trails real time; clamped to >= 1. */
    void SetStretchFactor(float factor);

    /** @brief Spectral pitch shift ratio, clamped to [kMinPitchRatio, kMaxPitchRatio]. */
    void SetPitchRatio(float ratio);

    /** @brief Stop recording; the playhead keeps cycling the frozen history. */
    void SetFreeze(bool frozen);

    /** @brief Snap the playhead to the most recent input (same as an onset). */
    void TriggerCatchUp() {
        catchUpPending_.store(true, std::memory_order_relaxed);
    }

    size_t WindowSize() const { return windowSize_; }
    float StretchFactor() const { return stretchFactor_; }
    float PitchRatio() const { return pitchRatio_; }
    bool IsFrozen() const { return frozen_.load(std::memory_order_relaxed); }

    /** @brief Total samples zero-padded by ReadOutput(); diagnostic. */
    uint32_t UnderrunCount() const {
        return underruns_.load(std::memory_order_relaxed);
    }

    /** @brief Playhead snaps performed (onset or manual); diagnostic. */
    uint32_t SnapCount() const { return snapCount_; }

    /** @brief Diagnostic: strongest analysis bin above ~2 kHz, pre-gate. */
    size_t ToneBin() const { return toneBin_; }
    /** @brief Diagnostic: magnitude of ToneBin() (full-scale sine ~ N/4). */
    float ToneMagnitude() const { return toneMagnitude_; }

    static constexpr size_t kMinWindowSize = 64;
    /** Smallest record ring that keeps the playhead guards consistent. */
    static constexpr size_t kMinRecordSize = 1024;
    /** Grain hop is windowSize / 2 (50% overlap, paulstretch style). */
    static constexpr size_t kHopDivisor = 2;
    static constexpr float kMinStretchFactor = 1.0f;
    static constexpr float kMinPitchRatio = 0.25f;
    static constexpr float kMaxPitchRatio = 4.0f;

private:
    bool ValidateBuffers(const SpectralStretchBuffers& buffers) const;
    bool ConfigureWindow(size_t size);
    void ResyncPlayhead(uint32_t writeHead);
    void SynthesizeGrain(uint32_t writeHead);
    void ApplySpectralProcessing();
    float NextRandomPhase();

    /** Margin the playhead keeps behind unwritten data, samples. */
    static constexpr uint32_t kDataMargin = 64;
    /** Distance from the record head after a hard resync, samples. */
    static constexpr uint32_t kResyncGuard = 1024;
    /** Seconds of frozen history the playhead cycles through. */
    static constexpr float kFreezeLoopSeconds = 4.0f;
    /** Output FIFO headroom kept on top of one grain hop. */
    static constexpr size_t kTargetFillSlack = 2048;
    /** Upper bound on grains synthesized per Update() call. Kept small so
     *  the caller can interleave several channels at grain granularity. */
    static constexpr size_t kMaxGrainsPerUpdate = 2;

    // Onset detector (runs per sample in WriteInput). The slow baseline
    // rises faster than it falls so a sustained tone stops counting as an
    // onset within ~0.25 s, while a hit after a quiet stretch still fires.
    static constexpr float kOnsetFastAttack = 0.02f;    ///< ~1 ms at 48 kHz
    static constexpr float kOnsetFastRelease = 0.0005f; ///< ~40 ms
    static constexpr float kOnsetSlowRise = 0.0002f;    ///< ~100 ms
    static constexpr float kOnsetSlowFall = 0.00005f;   ///< ~400 ms
    static constexpr float kOnsetRatio = 2.5f;          ///< fast / slow trigger
    static constexpr float kOnsetFloor = 0.01f;         ///< about -40 dBFS
    static constexpr uint32_t kOnsetRefractorySamples = 8192; ///< ~170 ms

    /** Spectral gate threshold as a fraction of windowSize; 1.25e-4 mutes
     *  bins below about -66 dBFS (relative to a full-scale sine). Tune by
     *  ear: too low lets codec/supply whine ring in the wash, too high eats
     *  quiet tails. */
    static constexpr float kSpectralGatePerWindow = 1.25e-4f;

    // Adaptive per-bin noise floor: rises toward a persistent magnitude
    // with ~2-6 s time constant (grain-rate dependent), falls quickly when
    // the bin drops, and is capped at about -40 dBFS so musical content
    // above roughly -36 dBFS can never be gated by it.
    static constexpr float kNoiseFloorRise = 0.02f;   ///< per grain
    static constexpr float kNoiseFloorFall = 0.2f;    ///< per grain
    static constexpr float kNoiseFloorRatio = 1.5f;   ///< gate at ratio*floor
    static constexpr float kNoiseFloorCapPerWindow = 2.5e-3f; ///< ~-40 dBFS

    SpectralStretchBuffers buffers_{};
    Fft fft_;
    SpscRingBuffer output_;

    float sampleRate_ = 0.0f;
    size_t windowSize_ = 0;
    size_t hopSize_ = 0;
    uint32_t recordMask_ = 0;

    float stretchFactor_ = kMinStretchFactor;
    float pitchRatio_ = 1.0f;

    /// Monotonic 32-bit stream position of the record head (wrap-safe).
    std::atomic<uint32_t> recordWrite_{0};
    std::atomic<bool> frozen_{false};
    std::atomic<uint32_t> underruns_{0};
    /// Manual catch-up request (unconditional snap); set from any context,
    /// consumed by the synthesis side.
    std::atomic<bool> catchUpPending_{false};
    /// Detector onset (snaps only when the playhead drifted far enough);
    /// set by the audio side, consumed by the synthesis side.
    std::atomic<bool> onsetPending_{false};

    // Audio-callback-only onset detector state.
    float onsetFastEnv_ = 0.0f;
    float onsetSlowEnv_ = 0.0f;
    uint32_t onsetHold_ = 0;

    /// Playhead as integer stream position plus fractional remainder.
    uint32_t playPosition_ = 0;
    float playFraction_ = 0.0f;
    /// Post-onset advance credit, samples; while nonzero the playhead rides
    /// the edge of the recorded data so the transient sweeps into the window.
    float onsetCredit_ = 0.0f;
    uint32_t snapCount_ = 0;
    size_t toneBin_ = 0;
    float toneMagnitude_ = 0.0f;

    uint32_t rngState_ = 1;
};

} // namespace ninetysix
