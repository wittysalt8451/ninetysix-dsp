#include "doctest/doctest.h"

#include "effects/spectral/SpectralStretch.h"
#include "utils/Fft.h"
#include "utils/SpscRingBuffer.h"

#include <cmath>
#include <vector>

using namespace sudwalfulkaan;

namespace {
constexpr float kTestTwoPi = 6.28318530717958647692f;
}

TEST_CASE("Fft rejects invalid configuration")
{
    Fft fft;
    std::vector<float> cosTable(8), sinTable(8);
    CHECK_FALSE(fft.Init(0, cosTable.data(), sinTable.data()));
    CHECK_FALSE(fft.Init(3, cosTable.data(), sinTable.data()));
    CHECK_FALSE(fft.Init(16, nullptr, sinTable.data()));
    CHECK(fft.Init(16, cosTable.data(), sinTable.data()));
    CHECK(fft.Size() == 16);
}

TEST_CASE("Fft of an impulse is flat")
{
    constexpr size_t kSize = 64;
    Fft fft;
    std::vector<float> cosTable(kSize / 2), sinTable(kSize / 2);
    REQUIRE(fft.Init(kSize, cosTable.data(), sinTable.data()));

    std::vector<float> real(kSize, 0.0f), imag(kSize, 0.0f);
    real[0] = 1.0f;
    fft.Forward(real.data(), imag.data());

    for (size_t k = 0; k < kSize; ++k) {
        CHECK(real[k] == doctest::Approx(1.0f).epsilon(1e-4));
        CHECK(imag[k] == doctest::Approx(0.0f).epsilon(1e-4));
    }
}

TEST_CASE("Fft locates a pure sine in the expected bin")
{
    constexpr size_t kSize = 256;
    constexpr size_t kBin = 10;
    Fft fft;
    std::vector<float> cosTable(kSize / 2), sinTable(kSize / 2);
    REQUIRE(fft.Init(kSize, cosTable.data(), sinTable.data()));

    std::vector<float> real(kSize), imag(kSize, 0.0f);
    for (size_t n = 0; n < kSize; ++n) {
        real[n] = std::sin(kTestTwoPi * static_cast<float>(kBin)
                           * static_cast<float>(n) / kSize);
    }
    fft.Forward(real.data(), imag.data());

    for (size_t k = 0; k <= kSize / 2; ++k) {
        const float magnitude
            = std::sqrt(real[k] * real[k] + imag[k] * imag[k]);
        const float expected = (k == kBin) ? kSize / 2.0f : 0.0f;
        CHECK(magnitude == doctest::Approx(expected).epsilon(1e-3));
    }
}

TEST_CASE("Fft forward followed by inverse is identity")
{
    constexpr size_t kSize = 512;
    Fft fft;
    std::vector<float> cosTable(kSize / 2), sinTable(kSize / 2);
    REQUIRE(fft.Init(kSize, cosTable.data(), sinTable.data()));

    std::vector<float> real(kSize), imag(kSize, 0.0f), original(kSize);
    for (size_t n = 0; n < kSize; ++n) {
        // Deterministic pseudo-random-ish signal
        original[n] = std::sin(0.1f * n) + 0.5f * std::cos(0.37f * n);
        real[n] = original[n];
    }

    fft.Forward(real.data(), imag.data());
    fft.Inverse(real.data(), imag.data());

    for (size_t n = 0; n < kSize; ++n) {
        CHECK(real[n] == doctest::Approx(original[n]).epsilon(1e-4));
        CHECK(imag[n] == doctest::Approx(0.0f).epsilon(1e-4));
    }
}

TEST_CASE("SpscRingBuffer basic write/read and wrap-around")
{
    constexpr size_t kCapacity = 16;
    std::vector<float> storage(kCapacity);
    SpscRingBuffer ring;

    CHECK_FALSE(ring.Init(nullptr, kCapacity));
    CHECK_FALSE(ring.Init(storage.data(), 12)); // not a power of two
    REQUIRE(ring.Init(storage.data(), kCapacity));

    CHECK(ring.ReadAvailable() == 0);
    CHECK(ring.WriteAvailable() == kCapacity);

    const float input[6] = {1, 2, 3, 4, 5, 6};
    float output[6] = {};

    // Many passes to force the indices around the ring repeatedly.
    for (int pass = 0; pass < 100; ++pass) {
        CHECK(ring.Write(input, 6) == 6);
        CHECK(ring.ReadAvailable() == 6);
        CHECK(ring.Read(output, 6) == 6);
        for (int i = 0; i < 6; ++i) {
            CHECK(output[i] == input[i]);
        }
    }
}

TEST_CASE("SpscRingBuffer limits writes to free space and reads to fill")
{
    constexpr size_t kCapacity = 8;
    std::vector<float> storage(kCapacity);
    SpscRingBuffer ring;
    REQUIRE(ring.Init(storage.data(), kCapacity));

    const float input[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    float output[12] = {};

    CHECK(ring.Write(input, 12) == kCapacity);
    CHECK(ring.WriteAvailable() == 0);
    CHECK(ring.Read(output, 12) == kCapacity);
    CHECK(ring.Read(output, 12) == 0);
}

namespace {

/** Host-side storage matching SpectralStretchBuffers requirements. */
struct HostStretchStorage {
    static constexpr size_t kMaxWindow = 1024;
    static constexpr size_t kRecord = 8192;
    static constexpr size_t kOutput = 4096;

    std::vector<float> record = std::vector<float>(kRecord);
    std::vector<float> grainReal = std::vector<float>(kMaxWindow);
    std::vector<float> grainImag = std::vector<float>(kMaxWindow);
    std::vector<float> window = std::vector<float>(kMaxWindow);
    std::vector<float> prevGrain = std::vector<float>(kMaxWindow / 2);
    std::vector<float> fadeOld = std::vector<float>(kMaxWindow / 2);
    std::vector<float> fadeNew = std::vector<float>(kMaxWindow / 2);
    std::vector<float> magnitude = std::vector<float>(kMaxWindow / 2 + 1);
    std::vector<float> noiseFloor = std::vector<float>(kMaxWindow / 2 + 1);
    std::vector<float> twiddleCos = std::vector<float>(kMaxWindow / 2);
    std::vector<float> twiddleSin = std::vector<float>(kMaxWindow / 2);
    std::vector<float> outputRing = std::vector<float>(kOutput);

    SpectralStretchBuffers Buffers() {
        SpectralStretchBuffers b;
        b.record = record.data();
        b.recordSize = kRecord;
        b.grainReal = grainReal.data();
        b.grainImag = grainImag.data();
        b.window = window.data();
        b.prevGrain = prevGrain.data();
        b.fadeOld = fadeOld.data();
        b.fadeNew = fadeNew.data();
        b.magnitude = magnitude.data();
        b.noiseFloor = noiseFloor.data();
        b.twiddleCos = twiddleCos.data();
        b.twiddleSin = twiddleSin.data();
        b.outputRing = outputRing.data();
        b.outputRingSize = kOutput;
        b.maxWindowSize = kMaxWindow;
        return b;
    }
};

/** Like HostStretchStorage but with a 2.7 s record ring, for tests that
 *  need the playhead to drift without hitting overwrite protection. */
struct HostStretchStorageBigRing {
    static constexpr size_t kMaxWindow = 1024;
    static constexpr size_t kRecord = 1 << 17;
    static constexpr size_t kOutput = 4096;

    std::vector<float> record = std::vector<float>(kRecord);
    std::vector<float> grainReal = std::vector<float>(kMaxWindow);
    std::vector<float> grainImag = std::vector<float>(kMaxWindow);
    std::vector<float> window = std::vector<float>(kMaxWindow);
    std::vector<float> prevGrain = std::vector<float>(kMaxWindow / 2);
    std::vector<float> fadeOld = std::vector<float>(kMaxWindow / 2);
    std::vector<float> fadeNew = std::vector<float>(kMaxWindow / 2);
    std::vector<float> magnitude = std::vector<float>(kMaxWindow / 2 + 1);
    std::vector<float> noiseFloor = std::vector<float>(kMaxWindow / 2 + 1);
    std::vector<float> twiddleCos = std::vector<float>(kMaxWindow / 2);
    std::vector<float> twiddleSin = std::vector<float>(kMaxWindow / 2);
    std::vector<float> outputRing = std::vector<float>(kOutput);

    SpectralStretchBuffers Buffers() {
        SpectralStretchBuffers b;
        b.record = record.data();
        b.recordSize = kRecord;
        b.grainReal = grainReal.data();
        b.grainImag = grainImag.data();
        b.window = window.data();
        b.prevGrain = prevGrain.data();
        b.fadeOld = fadeOld.data();
        b.fadeNew = fadeNew.data();
        b.magnitude = magnitude.data();
        b.noiseFloor = noiseFloor.data();
        b.twiddleCos = twiddleCos.data();
        b.twiddleSin = twiddleSin.data();
        b.outputRing = outputRing.data();
        b.outputRingSize = kOutput;
        b.maxWindowSize = kMaxWindow;
        return b;
    }
};

constexpr float kTestSampleRate = 48000.0f;
constexpr uint32_t kTestSeed = 0x12345678u;

/** Run input/update/output cycles and return the output RMS of the tail. */
float RunStretchRms(SpectralStretch& stretch, float inputFrequency,
                    size_t totalSamples, size_t measureTail)
{
    constexpr size_t kBlock = 48;
    float inputBlock[kBlock];
    float outputBlock[kBlock];

    double sumSquares = 0.0;
    size_t measured = 0;
    size_t produced = 0;

    for (size_t n = 0; n < totalSamples; n += kBlock) {
        for (size_t i = 0; i < kBlock; ++i) {
            inputBlock[i] = std::sin(kTestTwoPi * inputFrequency
                                     * static_cast<float>(n + i)
                                     / kTestSampleRate);
        }
        stretch.WriteInput(inputBlock, kBlock);
        stretch.Update();
        stretch.ReadOutput(outputBlock, kBlock);

        produced += kBlock;
        if (totalSamples - produced < measureTail) {
            for (size_t i = 0; i < kBlock; ++i) {
                sumSquares += static_cast<double>(outputBlock[i])
                            * static_cast<double>(outputBlock[i]);
                ++measured;
            }
        }
    }
    return measured > 0
        ? static_cast<float>(std::sqrt(sumSquares / measured))
        : 0.0f;
}

} // namespace

TEST_CASE("SpectralStretch rejects incomplete buffer sets")
{
    HostStretchStorage storage;
    SpectralStretch stretch;

    SpectralStretchBuffers broken = storage.Buffers();
    broken.record = nullptr;
    CHECK_FALSE(stretch.Init(kTestSampleRate, broken, kTestSeed));

    broken = storage.Buffers();
    broken.recordSize = 100; // not a power of two
    CHECK_FALSE(stretch.Init(kTestSampleRate, broken, kTestSeed));

    CHECK(stretch.Init(kTestSampleRate, storage.Buffers(), kTestSeed));
}

TEST_CASE("SpectralStretch produces sustained, bounded output from a sine")
{
    HostStretchStorage storage;
    SpectralStretch stretch;
    REQUIRE(stretch.Init(kTestSampleRate, storage.Buffers(), kTestSeed));
    REQUIRE(stretch.SetWindowSize(512));
    stretch.SetStretchFactor(8.0f);

    constexpr size_t kTotal = 48000;
    constexpr size_t kTail = 24000;
    const float rms = RunStretchRms(stretch, 1000.0f, kTotal, kTail);

    // A unit sine has RMS ~0.707; the stretched output keeps the magnitude
    // spectrum, so the steady-state level must be in the same ballpark.
    CHECK(rms > 0.2f);
    CHECK(rms < 1.5f);
}

TEST_CASE("SpectralStretch keeps producing output while frozen")
{
    HostStretchStorage storage;
    SpectralStretch stretch;
    REQUIRE(stretch.Init(kTestSampleRate, storage.Buffers(), kTestSeed));
    REQUIRE(stretch.SetWindowSize(512));
    stretch.SetStretchFactor(4.0f);

    // Capture some material, then freeze.
    RunStretchRms(stretch, 440.0f, 24000, 1);
    stretch.SetFreeze(true);
    CHECK(stretch.IsFrozen());

    constexpr size_t kBlock = 48;
    float silence[kBlock] = {};
    float outputBlock[kBlock];
    double sumSquares = 0.0;
    size_t count = 0;

    // Long enough that the frozen playhead must wrap its loop span.
    for (size_t n = 0; n < 96000; n += kBlock) {
        stretch.WriteInput(silence, kBlock); // must be ignored while frozen
        stretch.Update();
        stretch.ReadOutput(outputBlock, kBlock);
        if (n > 48000) {
            for (size_t i = 0; i < kBlock; ++i) {
                sumSquares += static_cast<double>(outputBlock[i])
                            * static_cast<double>(outputBlock[i]);
                ++count;
            }
        }
    }
    const float rms = static_cast<float>(std::sqrt(sumSquares / count));
    CHECK(rms > 0.1f);
}

TEST_CASE("SpectralStretch window size changes are validated and applied")
{
    HostStretchStorage storage;
    SpectralStretch stretch;
    REQUIRE(stretch.Init(kTestSampleRate, storage.Buffers(), kTestSeed));

    CHECK_FALSE(stretch.SetWindowSize(HostStretchStorage::kMaxWindow * 2));
    CHECK_FALSE(stretch.SetWindowSize(300)); // not a power of two
    CHECK(stretch.SetWindowSize(HostStretchStorage::kMaxWindow));
    CHECK(stretch.WindowSize() == HostStretchStorage::kMaxWindow);

    // Still produces audio after the change.
    const float rms = RunStretchRms(stretch, 500.0f, 96000, 24000);
    CHECK(rms > 0.2f);
}

TEST_CASE("SpectralStretch responds promptly to a transient after silence")
{
    HostStretchStorage storage;
    SpectralStretch stretch;
    REQUIRE(stretch.Init(kTestSampleRate, storage.Buffers(), kTestSeed));
    REQUIRE(stretch.SetWindowSize(512));
    stretch.SetStretchFactor(32.0f);

    constexpr size_t kBlock = 48;
    float inputBlock[kBlock] = {};
    float outputBlock[kBlock];

    // Half a second of silence: the playhead crawls, the input envelope
    // decays to inactive.
    for (size_t n = 0; n < 24000; n += kBlock) {
        stretch.WriteInput(inputBlock, kBlock);
        stretch.Update();
        stretch.ReadOutput(outputBlock, kBlock);
    }

    // A loud sine starts: the onset detector must snap the playhead so the
    // output carries real energy within a few thousand samples despite the
    // 32x stretch.
    double sumSquares = 0.0;
    size_t count = 0;
    for (size_t n = 0; n < 12000; n += kBlock) {
        for (size_t i = 0; i < kBlock; ++i) {
            inputBlock[i] = 0.9f * std::sin(kTestTwoPi * 880.0f
                                            * static_cast<float>(n + i)
                                            / kTestSampleRate);
        }
        stretch.WriteInput(inputBlock, kBlock);
        stretch.Update();
        stretch.ReadOutput(outputBlock, kBlock);
        if (n >= 6000) {
            for (size_t i = 0; i < kBlock; ++i) {
                sumSquares += static_cast<double>(outputBlock[i])
                            * static_cast<double>(outputBlock[i]);
                ++count;
            }
        }
    }
    const float rms = static_cast<float>(std::sqrt(sumSquares / count));
    CHECK(rms > 0.05f);
}

TEST_CASE("SpectralStretch onset does not re-anchor before the stretch-scaled drift")
{
    HostStretchStorageBigRing storage;
    SpectralStretch stretch;
    REQUIRE(stretch.Init(kTestSampleRate, storage.Buffers(), kTestSeed));
    REQUIRE(stretch.SetWindowSize(512));
    stretch.SetStretchFactor(64.0f);

    constexpr size_t kBlock = 48;
    float inputBlock[kBlock];
    float outputBlock[kBlock];

    // Feed a loud 220 Hz tone for 0.25 s: the playhead lag stays below the
    // snap threshold (window + stretch * hop = 16896 samples).
    for (size_t n = 0; n < 12000; n += kBlock) {
        for (size_t i = 0; i < kBlock; ++i) {
            inputBlock[i] = 0.9f * std::sin(kTestTwoPi * 220.0f
                                            * static_cast<float>(n + i)
                                            / kTestSampleRate);
        }
        stretch.WriteInput(inputBlock, kBlock);
        stretch.Update();
        stretch.ReadOutput(outputBlock, kBlock);
    }

    // Hard switch to 1760 Hz after a short gap (a clear onset). Because the
    // drift is still small, the playhead must NOT snap: at 64x stretch the
    // output stays 220 Hz-dominated for the measured stretch of time.
    float silence[kBlock] = {};
    for (size_t n = 0; n < 2400; n += kBlock) {
        stretch.WriteInput(silence, kBlock);
        stretch.Update();
        stretch.ReadOutput(outputBlock, kBlock);
    }
    size_t crossings = 0, samples = 0;
    float previous = 0.0f;
    for (size_t n = 0; n < 12000; n += kBlock) {
        for (size_t i = 0; i < kBlock; ++i) {
            inputBlock[i] = 0.9f * std::sin(kTestTwoPi * 1760.0f
                                            * static_cast<float>(n + i)
                                            / kTestSampleRate);
        }
        stretch.WriteInput(inputBlock, kBlock);
        stretch.Update();
        stretch.ReadOutput(outputBlock, kBlock);
        // Measure after the FIFO turned over but before the detector could
        // legitimately re-fire (refractory) with the drift gone large.
        if (n >= 4800 && n < 9600) {
            for (size_t i = 0; i < kBlock; ++i) {
                if ((outputBlock[i] > 0.0f) != (previous > 0.0f)) {
                    ++crossings;
                }
                previous = outputBlock[i];
                ++samples;
            }
        }
    }
    // 220 Hz-ish content -> roughly 440 crossings/s; 1760 Hz would be ~3520.
    const float crossingsPerSecond = static_cast<float>(crossings)
        / (static_cast<float>(samples) / kTestSampleRate);
    CHECK(crossingsPerSecond < 1500.0f);
}

TEST_CASE("SpectralStretch TriggerCatchUp snaps the playhead forward")
{
    HostStretchStorage storage;
    SpectralStretch stretch;
    REQUIRE(stretch.Init(kTestSampleRate, storage.Buffers(), kTestSeed));
    REQUIRE(stretch.SetWindowSize(512));
    stretch.SetStretchFactor(32.0f);

    // Build up material, then let the playhead fall behind on quiet input
    // (below the onset floor, so only the manual trigger can snap it).
    RunStretchRms(stretch, 220.0f, 24000, 1);

    constexpr size_t kBlock = 48;
    float inputBlock[kBlock];
    float outputBlock[kBlock];

    stretch.TriggerCatchUp();
    double sumSquares = 0.0;
    size_t count = 0;
    for (size_t n = 0; n < 12000; n += kBlock) {
        for (size_t i = 0; i < kBlock; ++i) {
            inputBlock[i] = 0.5f * std::sin(kTestTwoPi * 660.0f
                                            * static_cast<float>(n + i)
                                            / kTestSampleRate);
        }
        stretch.WriteInput(inputBlock, kBlock);
        stretch.Update();
        stretch.ReadOutput(outputBlock, kBlock);
        for (size_t i = 0; i < kBlock; ++i) {
            sumSquares += static_cast<double>(outputBlock[i])
                        * static_cast<double>(outputBlock[i]);
            ++count;
        }
    }
    const float rms = static_cast<float>(std::sqrt(sumSquares / count));
    CHECK(rms > 0.05f);
}

TEST_CASE("SpectralStretch parameter setters clamp to valid ranges")
{
    HostStretchStorage storage;
    SpectralStretch stretch;
    REQUIRE(stretch.Init(kTestSampleRate, storage.Buffers(), kTestSeed));

    stretch.SetStretchFactor(0.1f);
    CHECK(stretch.StretchFactor() == SpectralStretch::kMinStretchFactor);

    stretch.SetPitchRatio(100.0f);
    CHECK(stretch.PitchRatio() == SpectralStretch::kMaxPitchRatio);
    stretch.SetPitchRatio(0.0f);
    CHECK(stretch.PitchRatio() == SpectralStretch::kMinPitchRatio);
}
