#include "doctest/doctest.h"

#include "effects/reverb/FDN4Reverb.h"

#include <cmath>
#include <vector>

using ninetysix::FDN4Reverb;

namespace {
    constexpr float kSampleRate = 48000.0f;
    float reverb_buffer[FDN4Reverb::kBufferSize];

    // Deterministic noise in [-0.5, 0.5]
    float NextNoise(uint32_t& state) {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state >> 8) /
                   static_cast<float>(1u << 24) - 0.5f;
    }

    float Rms(const std::vector<float>& x, size_t begin, size_t end) {
        float sum = 0.0f;
        for (size_t i = begin; i < end; i++) sum += x[i] * x[i];
        return sqrtf(sum / static_cast<float>(end - begin));
    }
}

TEST_CASE("FDN4Reverb passes dry signal untouched at mix 0")
{
    using doctest::Approx;
    FDN4Reverb rv;
    rv.Init(kSampleRate, reverb_buffer, FDN4Reverb::kBufferSize);
    rv.SetMix(0.0f);

    uint32_t seed = 1;
    for (int i = 0; i < 1000; i++) {
        const float inL = NextNoise(seed);
        const float inR = NextNoise(seed);
        float outL, outR;
        rv.Process(inL, inR, &outL, &outR);
        CHECK(outL == Approx(inL).epsilon(1e-4));
        CHECK(outR == Approx(inR).epsilon(1e-4));
    }
}

TEST_CASE("FDN4Reverb is silent for silent input")
{
    FDN4Reverb rv;
    rv.Init(kSampleRate, reverb_buffer, FDN4Reverb::kBufferSize);
    rv.SetMix(1.0f);

    for (int i = 0; i < 4800; i++) {
        float outL, outR;
        rv.Process(0.0f, 0.0f, &outL, &outR);
        CHECK(outL == 0.0f);
        CHECK(outR == 0.0f);
    }
}

TEST_CASE("FDN4Reverb stays bounded and decays at maximum settings")
{
    FDN4Reverb rv;
    rv.Init(kSampleRate, reverb_buffer, FDN4Reverb::kBufferSize);
    rv.SetDecay(1.0f);
    rv.SetDamping(1.0f);
    rv.SetModulation(1.0f);
    rv.SetMix(1.0f);

    // Half a second of noise, then three seconds of silence
    const size_t excite = static_cast<size_t>(0.5f * kSampleRate);
    const size_t tail = static_cast<size_t>(3.0f * kSampleRate);
    uint32_t seed = 42;
    std::vector<float> wetL(tail);
    float outL, outR;
    for (size_t i = 0; i < excite; i++) {
        rv.Process(NextNoise(seed), NextNoise(seed), &outL, &outR);
        REQUIRE(std::isfinite(outL));
        REQUIRE(std::isfinite(outR));
        REQUIRE(fabsf(outL) <= 1.0f);
        REQUIRE(fabsf(outR) <= 1.0f);
    }
    for (size_t i = 0; i < tail; i++) {
        rv.Process(0.0f, 0.0f, &outL, &outR);
        REQUIRE(std::isfinite(outL));
        REQUIRE(fabsf(outL) <= 1.0f);
        wetL[i] = outL;
    }

    // No runaway: the tail must lose energy over time even at Decay = 1
    const size_t quarter = tail / 4;
    const float early = Rms(wetL, 0, quarter);
    const float late = Rms(wetL, tail - quarter, tail);
    CHECK(early > 0.0f);
    CHECK(late < 0.25f * early);
}

TEST_CASE("FDN4Reverb decay parameter lengthens the tail")
{
    auto TailRms = [](float decay) {
        FDN4Reverb rv;
        rv.Init(kSampleRate, reverb_buffer, FDN4Reverb::kBufferSize);
        rv.SetDecay(decay);
        rv.SetMix(1.0f);
        float outL, outR;
        rv.Process(1.0f, 1.0f, &outL, &outR);
        // Measure the impulse tail between 0.5 s and 1.0 s
        const size_t skip = static_cast<size_t>(0.5f * kSampleRate);
        std::vector<float> tail(skip);
        for (size_t i = 0; i < skip; i++) rv.Process(0.0f, 0.0f, &outL, &outR);
        for (size_t i = 0; i < skip; i++) {
            rv.Process(0.0f, 0.0f, &outL, &outR);
            tail[i] = outL;
        }
        return Rms(tail, 0, tail.size());
    };

    const float short_tail = TailRms(0.1f);
    const float long_tail = TailRms(0.9f);
    CHECK(long_tail > 4.0f * short_tail);
}

TEST_CASE("FDN4Reverb output is decorrelated stereo")
{
    FDN4Reverb rv;
    rv.Init(kSampleRate, reverb_buffer, FDN4Reverb::kBufferSize);
    rv.SetDecay(0.8f);
    rv.SetMix(1.0f);

    // Mono input: both channels get the identical signal
    const size_t n = static_cast<size_t>(1.0f * kSampleRate);
    uint32_t seed = 7;
    std::vector<float> wetL(n), wetR(n);
    for (size_t i = 0; i < n; i++) {
        const float mono = i < n / 4 ? NextNoise(seed) : 0.0f;
        rv.Process(mono, mono, &wetL[i], &wetR[i]);
    }

    float ll = 0.0f, rr = 0.0f, lr = 0.0f;
    for (size_t i = 0; i < n; i++) {
        ll += wetL[i] * wetL[i];
        rr += wetR[i] * wetR[i];
        lr += wetL[i] * wetR[i];
    }
    REQUIRE(ll > 0.0f);
    REQUIRE(rr > 0.0f);
    const float correlation = lr / sqrtf(ll * rr);
    // Clearly different channels, but no anti-phase mono cancellation
    CHECK(correlation < 0.9f);
    CHECK(correlation > -0.5f);
}

TEST_CASE("FDN4Reverb degrades gracefully with a small buffer")
{
    FDN4Reverb rv;
    rv.Init(kSampleRate, reverb_buffer, FDN4Reverb::kBufferSize / 2);
    rv.SetDecay(1.0f);
    rv.SetMix(1.0f);

    uint32_t seed = 3;
    float outL, outR;
    bool any_wet = false;
    for (int i = 0; i < 48000; i++) {
        rv.Process(NextNoise(seed), NextNoise(seed), &outL, &outR);
        REQUIRE(std::isfinite(outL));
        REQUIRE(std::isfinite(outR));
        REQUIRE(fabsf(outL) <= 1.0f);
        any_wet = any_wet || outL != 0.0f;
    }
    CHECK(any_wet);
}

TEST_CASE("FDN4Reverb does not clip a hot input")
{
    // A full-scale brassy tone (six harmonics) at a long decay sums over
    // 1.0; it must come out unclipped for the limiter after it, not
    // flattened at 1.0
    FDN4Reverb rv;
    rv.Init(kSampleRate, reverb_buffer, FDN4Reverb::kBufferSize);
    rv.SetDecay(0.8f);
    rv.SetMix(0.8f);
    float peak = 0.0f;
    int at_one = 0;
    float outL, outR;
    for (int n = 0; n < static_cast<int>(kSampleRate); n++) {
        float x = 0.0f;
        for (int h = 1; h <= 6; h++) {
            x += std::sin(6.2831853f * 233.0f * h * n / kSampleRate) / h;
        }
        x /= 1.35f;  // peak of the sum: full scale
        rv.Process(x, x, &outL, &outR);
        REQUIRE(std::isfinite(outL));
        peak = std::fmax(peak, std::fabs(outL));
        if (std::fabs(outL) == 1.0f) at_one++;
    }
    CHECK(peak > 1.2f);
    CHECK(at_one == 0);
}
