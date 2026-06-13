#include "effects/spectral/SpectralStretch.h"

#include <cmath>
#include <cstring>

// The synthesis stage (50% hop, raised-cosine crossfade of un-windowed
// random-phase grains, equal-power compensation curve) follows the
// paulstretch algorithm by Nasca Octavian Paul.

namespace sudwalfulkaan {

namespace {
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kPi = 3.14159265358979323846f;
/// Maps a full 32-bit random word onto [0, 2*pi).
constexpr float kPhasePerRandomWord = kTwoPi / 4294967296.0f;
/// Equal-power crossfade compensation midpoint: (1 + 1/sqrt(2)) / 2.
constexpr float kHalfInvSqrt2 = 0.853553390593f;
/// Output amplitude factor, matches paulstretch.
constexpr float kAmpFactor = 2.0f;
} // namespace

bool SpectralStretch::Init(float sampleRate,
                           const SpectralStretchBuffers& buffers,
                           uint32_t rngSeed) {
    if (sampleRate <= 0.0f || !ValidateBuffers(buffers)) {
        return false;
    }

    buffers_ = buffers;
    sampleRate_ = sampleRate;
    recordMask_ = static_cast<uint32_t>(buffers_.recordSize - 1);
    rngState_ = rngSeed != 0 ? rngSeed : 1;

    std::memset(buffers_.record, 0, buffers_.recordSize * sizeof(float));
    if (!output_.Init(buffers_.outputRing, buffers_.outputRingSize)) {
        return false;
    }

    // Start the stream one full ring in, so the zeroed history behind the
    // record head reads as valid (silent) audio.
    recordWrite_.store(static_cast<uint32_t>(buffers_.recordSize),
                       std::memory_order_relaxed);
    frozen_.store(false, std::memory_order_relaxed);
    underruns_.store(0, std::memory_order_relaxed);
    catchUpPending_.store(false, std::memory_order_relaxed);
    onsetPending_.store(false, std::memory_order_relaxed);
    onsetFastEnv_ = 0.0f;
    onsetSlowEnv_ = 0.0f;
    onsetHold_ = 0;
    onsetCredit_ = 0.0f;
    snapCount_ = 0;
    stretchFactor_ = kMinStretchFactor;
    pitchRatio_ = 1.0f;

    const size_t defaultWindowSize = buffers_.maxWindowSize / 4;
    return ConfigureWindow(defaultWindowSize >= kMinWindowSize
                               ? defaultWindowSize
                               : kMinWindowSize);
}

bool SpectralStretch::ValidateBuffers(
    const SpectralStretchBuffers& buffers) const {
    const bool pointersValid = buffers.record != nullptr
        && buffers.grainReal != nullptr && buffers.grainImag != nullptr
        && buffers.window != nullptr && buffers.prevGrain != nullptr
        && buffers.fadeOld != nullptr && buffers.fadeNew != nullptr
        && buffers.magnitude != nullptr && buffers.noiseFloor != nullptr
        && buffers.twiddleCos != nullptr && buffers.twiddleSin != nullptr
        && buffers.outputRing != nullptr;
    if (!pointersValid) {
        return false;
    }
    if (!Fft::IsPowerOfTwo(buffers.maxWindowSize)
        || buffers.maxWindowSize < kMinWindowSize) {
        return false;
    }
    if (!Fft::IsPowerOfTwo(buffers.recordSize)
        || buffers.recordSize < kMinRecordSize
        || buffers.recordSize < 4 * buffers.maxWindowSize) {
        return false;
    }
    const size_t minOutputRing = buffers.maxWindowSize;
    if (!Fft::IsPowerOfTwo(buffers.outputRingSize)
        || buffers.outputRingSize < minOutputRing) {
        return false;
    }
    return true;
}

void SpectralStretch::WriteInput(const float* input, size_t count) {
    if (frozen_.load(std::memory_order_relaxed)) {
        return;
    }
    const uint32_t write = recordWrite_.load(std::memory_order_relaxed);

    float fast = onsetFastEnv_;
    float slow = onsetSlowEnv_;
    uint32_t hold = onsetHold_;
    bool onset = false;

    for (size_t i = 0; i < count; ++i) {
        const float sample = input[i];
        buffers_.record[(write + static_cast<uint32_t>(i)) & recordMask_]
            = sample;

        // Fast/slow envelope pair: a fast rise well above the slow baseline
        // marks a transient at the record head.
        const float level = sample < 0.0f ? -sample : sample;
        fast += (level - fast)
              * (level > fast ? kOnsetFastAttack : kOnsetFastRelease);
        slow += (level - slow)
              * (level > slow ? kOnsetSlowRise : kOnsetSlowFall);
        if (hold > 0) {
            --hold;
        } else if (fast > kOnsetFloor && fast > slow * kOnsetRatio) {
            onset = true;
            hold = kOnsetRefractorySamples;
        }
    }

    onsetFastEnv_ = fast;
    onsetSlowEnv_ = slow;
    onsetHold_ = hold;
    if (onset) {
        onsetPending_.store(true, std::memory_order_relaxed);
    }
    recordWrite_.store(write + static_cast<uint32_t>(count),
                       std::memory_order_release);
}

void SpectralStretch::ReadOutput(float* output, size_t count) {
    const size_t read = output_.Read(output, count);
    if (read < count) {
        std::memset(output + read, 0, (count - read) * sizeof(float));
        underruns_.fetch_add(static_cast<uint32_t>(count - read),
                             std::memory_order_relaxed);
    }
}

void SpectralStretch::Update() {
    const size_t capacity = output_.Capacity();
    size_t targetFill = hopSize_ + kTargetFillSlack;
    if (targetFill > capacity - hopSize_) {
        targetFill = capacity - hopSize_;
    }

    size_t grains = 0;
    while (output_.ReadAvailable() < targetFill
           && output_.WriteAvailable() >= hopSize_
           && grains < kMaxGrainsPerUpdate) {
        const uint32_t writeHead = recordWrite_.load(std::memory_order_acquire);
        const bool frozen = frozen_.load(std::memory_order_relaxed);

        // Manual catch-up snaps unconditionally; a detector onset only
        // re-anchors the playhead once it has drifted a stretch-scaled
        // distance behind, so the stretch factor stays audible on rhythmic
        // material instead of being reset by every hit.
        bool snap = catchUpPending_.exchange(false, std::memory_order_relaxed);
        if (onsetPending_.exchange(false, std::memory_order_relaxed)
            && !snap) {
            const uint32_t lag = writeHead - playPosition_;
            const float drift = static_cast<float>(windowSize_)
                + stretchFactor_ * static_cast<float>(hopSize_);
            snap = static_cast<float>(lag) > drift;
        }
        if (snap && !frozen) {
            // Snap to the newest fully recorded grain: the transient sits at
            // the (quiet) end of the analysis window, and the advance credit
            // sweeps it to the window center within half a window of real
            // time. Never requires unrecorded data, so synthesis can
            // continue immediately.
            const uint32_t target = writeHead
                - (static_cast<uint32_t>(windowSize_) + kDataMargin);
            if (target - playPosition_ < 0x80000000u) { // only snap forward
                playPosition_ = target;
                playFraction_ = 0.0f;
                onsetCredit_ = static_cast<float>(windowSize_) * 0.5f;
                ++snapCount_;
            }
        }

        if (!frozen) {
            const uint32_t lag = writeHead - playPosition_;
            if (lag < static_cast<uint32_t>(windowSize_) + kDataMargin) {
                break; // grain not fully recorded yet; retry next Update()
            }
        }

        SynthesizeGrain(writeHead);
        ++grains;
    }
}

bool SpectralStretch::SetWindowSize(size_t size) {
    if (size == windowSize_) {
        return true;
    }
    if (!Fft::IsPowerOfTwo(size) || size < kMinWindowSize
        || size > buffers_.maxWindowSize) {
        return false;
    }
    return ConfigureWindow(size);
}

void SpectralStretch::SetStretchFactor(float factor) {
    stretchFactor_ = factor < kMinStretchFactor ? kMinStretchFactor : factor;
}

void SpectralStretch::SetPitchRatio(float ratio) {
    if (ratio < kMinPitchRatio) {
        ratio = kMinPitchRatio;
    } else if (ratio > kMaxPitchRatio) {
        ratio = kMaxPitchRatio;
    }
    pitchRatio_ = ratio;
}

void SpectralStretch::SetFreeze(bool frozen) {
    const bool wasFrozen = frozen_.exchange(frozen, std::memory_order_relaxed);
    if (wasFrozen && !frozen) {
        // Recording resumes: snap the playhead back near the record head so
        // it does not linger in stale history.
        ResyncPlayhead(recordWrite_.load(std::memory_order_acquire));
    }
}

bool SpectralStretch::ConfigureWindow(size_t size) {
    if (!fft_.Init(size, buffers_.twiddleCos, buffers_.twiddleSin)) {
        return false;
    }
    const size_t hop = size / kHopDivisor;

    // Periodic Hann analysis window.
    const float phaseStep = kTwoPi / static_cast<float>(size);
    for (size_t i = 0; i < size; ++i) {
        buffers_.window[i]
            = 0.5f * (1.0f - std::cos(phaseStep * static_cast<float>(i)));
    }

    // Paulstretch output stage: a raised-cosine crossfade between the
    // previous and the current grain, times a curve that compensates the
    // power dip where two uncorrelated grains blend at equal gain.
    for (size_t i = 0; i < hop; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(hop);
        const float fade = 0.5f + 0.5f * std::cos(kPi * t);
        const float comp = kAmpFactor
            * (kHalfInvSqrt2 - (1.0f - kHalfInvSqrt2) * std::cos(kTwoPi * t));
        buffers_.fadeOld[i] = fade * comp;
        buffers_.fadeNew[i] = (1.0f - fade) * comp;
    }

    // The previous grain no longer matches the new hop length; fading in
    // from silence over one hop avoids a click, and the output FIFO is left
    // untouched so already-synthesized audio keeps playing.
    std::memset(buffers_.prevGrain, 0,
                (buffers_.maxWindowSize / 2) * sizeof(float));
    // Bin meanings change with the window size; re-learn the noise floor.
    std::memset(buffers_.noiseFloor, 0,
                (buffers_.maxWindowSize / 2 + 1) * sizeof(float));
    windowSize_ = size;
    hopSize_ = hop;
    ResyncPlayhead(recordWrite_.load(std::memory_order_acquire));
    return true;
}

void SpectralStretch::ResyncPlayhead(uint32_t writeHead) {
    playPosition_ = writeHead
        - (static_cast<uint32_t>(windowSize_) + kResyncGuard);
    playFraction_ = 0.0f;
}

void SpectralStretch::SynthesizeGrain(uint32_t writeHead) {
    const uint32_t windowSpan = static_cast<uint32_t>(windowSize_);
    const uint32_t lag = writeHead - playPosition_;
    const bool frozen = frozen_.load(std::memory_order_relaxed);

    if (frozen) {
        // Cycle the playhead through the most recent frozen history.
        if (lag < windowSpan) {
            const uint32_t maxSpan
                = static_cast<uint32_t>(buffers_.recordSize) - windowSpan;
            uint32_t loopSpan = static_cast<uint32_t>(
                kFreezeLoopSeconds * sampleRate_);
            if (loopSpan > maxSpan) {
                loopSpan = maxSpan;
            }
            playPosition_ -= loopSpan;
        }
    } else {
        // About to be overwritten (the elastic cap normally prevents this).
        const uint32_t maxLag
            = static_cast<uint32_t>(buffers_.recordSize) - kResyncGuard;
        if (lag > maxLag) {
            ResyncPlayhead(writeHead);
        }
    }

    // Windowed analysis grain.
    const uint32_t start = playPosition_;
    for (size_t i = 0; i < windowSize_; ++i) {
        const uint32_t index = (start + static_cast<uint32_t>(i)) & recordMask_;
        buffers_.grainReal[i] = buffers_.record[index] * buffers_.window[i];
        buffers_.grainImag[i] = 0.0f;
    }

    fft_.Forward(buffers_.grainReal, buffers_.grainImag);
    ApplySpectralProcessing();
    fft_.Inverse(buffers_.grainReal, buffers_.grainImag);

    // Crossfade the new grain against the previous one; the random-phase
    // grains have a flat envelope, so no synthesis window is needed.
    // grainImag is free after the inverse FFT and serves as scratch.
    float* mixed = buffers_.grainImag;
    for (size_t i = 0; i < hopSize_; ++i) {
        mixed[i] = buffers_.grainReal[hopSize_ + i] * buffers_.fadeNew[i]
                 + buffers_.prevGrain[i] * buffers_.fadeOld[i];
    }
    output_.Write(mixed, hopSize_);
    std::memcpy(buffers_.prevGrain, buffers_.grainReal,
                hopSize_ * sizeof(float));

    // Advance the playhead by hop / stretch with a fractional remainder.
    const float baseAdvance = static_cast<float>(hopSize_) / stretchFactor_;
    float advance = baseAdvance;
    if (!frozen) {
        // Post-onset credit: ride the edge of the recorded data so the
        // transient sweeps into the window at real-time rate, independent of
        // the stretch factor.
        if (onsetCredit_ > 0.0f) {
            const float boost = onsetCredit_ < static_cast<float>(hopSize_)
                                    ? onsetCredit_
                                    : static_cast<float>(hopSize_);
            advance += boost;
        }
        // Never advance into data that has not been recorded yet.
        const float maxAdvance = static_cast<float>(lag)
            - static_cast<float>(windowSize_) - static_cast<float>(kDataMargin);
        if (advance > maxAdvance) {
            advance = maxAdvance > 0.0f ? maxAdvance : 0.0f;
        }
        const float creditSpent = advance - baseAdvance;
        if (creditSpent > 0.0f) {
            onsetCredit_ -= creditSpent;
            if (onsetCredit_ < 0.0f) {
                onsetCredit_ = 0.0f;
            }
        }
    }
    playFraction_ += advance;
    const uint32_t step = static_cast<uint32_t>(playFraction_);
    playFraction_ -= static_cast<float>(step);
    playPosition_ += step;
}

void SpectralStretch::ApplySpectralProcessing() {
    float* real = buffers_.grainReal;
    float* imag = buffers_.grainImag;
    float* magnitude = buffers_.magnitude;
    const size_t half = windowSize_ / 2;

    // Spectral gate: phase randomization turns even faint steady tones
    // (codec hiss, supply whine) into a clearly audible constant beep. Mute
    // bins below the fixed floor, and additionally track each bin's own
    // stationary level so a constant whine above the fixed floor is learned
    // and muted as well, regardless of its exact level.
    const float gate = kSpectralGatePerWindow * static_cast<float>(windowSize_);
    const float floorCap
        = kNoiseFloorCapPerWindow * static_cast<float>(windowSize_);
    float* noiseFloor = buffers_.noiseFloor;
    // Diagnostic tone tracker: strongest pre-gate bin above ~2 kHz.
    const size_t toneStartBin
        = static_cast<size_t>(2000.0f * static_cast<float>(windowSize_)
                              / sampleRate_);
    size_t toneBin = 0;
    float toneMagnitude = 0.0f;
    for (size_t k = 0; k <= half; ++k) {
        const float m = std::sqrt(real[k] * real[k] + imag[k] * imag[k]);
        if (k >= toneStartBin && m > toneMagnitude) {
            toneMagnitude = m;
            toneBin = k;
        }

        float floor = noiseFloor[k];
        floor += (m - floor) * (m < floor ? kNoiseFloorFall : kNoiseFloorRise);
        if (floor > floorCap) {
            floor = floorCap;
        }
        noiseFloor[k] = floor;

        const float adaptive = kNoiseFloorRatio * floor;
        const float threshold = adaptive > gate ? adaptive : gate;
        magnitude[k] = m < threshold ? 0.0f : m;
    }
    toneBin_ = toneBin;
    toneMagnitude_ = toneMagnitude;

    // Zero DC and Nyquist so randomized grains cannot accumulate offset.
    real[0] = 0.0f;
    imag[0] = 0.0f;
    real[half] = 0.0f;
    imag[half] = 0.0f;

    // Pitch shift by resampling the magnitude spectrum, then discard all
    // phase information; Hermitian symmetry keeps the inverse FFT real.
    const float sourceStep = 1.0f / pitchRatio_;
    for (size_t k = 1; k < half; ++k) {
        const float source = static_cast<float>(k) * sourceStep;
        float shifted = 0.0f;
        if (source < static_cast<float>(half)) {
            const size_t index = static_cast<size_t>(source);
            const float fraction = source - static_cast<float>(index);
            shifted = magnitude[index]
                + (magnitude[index + 1] - magnitude[index]) * fraction;
        }
        const float phase = NextRandomPhase();
        const float re = shifted * std::cos(phase);
        const float im = shifted * std::sin(phase);
        real[k] = re;
        imag[k] = im;
        real[windowSize_ - k] = re;
        imag[windowSize_ - k] = -im;
    }
}

float SpectralStretch::NextRandomPhase() {
    // xorshift32: cheap, allocation-free, good enough for phase noise.
    uint32_t x = rngState_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rngState_ = x;
    return static_cast<float>(x) * kPhasePerRandomWord;
}

} // namespace sudwalfulkaan
