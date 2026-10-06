#include "doctest/doctest.h"

#include "effects/filters/StateVariableFilter.h"
#include "effects/saturation/Bitcrush.h"

#include <cmath>
#include <cstdint>

using ninetysix::Bitcrush;
using ninetysix::StateVariableFilter;

namespace {
    constexpr float kSampleRate = 48000.0f;
    constexpr float kTwoPi      = 6.28318530718f;

    float Db(float ratio) { return 20.0f * std::log10(ratio); }

    // Steady-state gain of one filter output for a sine at hz. The default
    // level is low enough that the drive does not touch the resonance.
    enum Output { LOW, HIGH, BAND };
    float SineGainDb(float cutoff, float res, Output output, float hz,
                     float level = 0.01f) {
        StateVariableFilter f;
        f.Init(kSampleRate);
        f.SetRes(res);
        f.SetFreq(cutoff);
        const int settle = static_cast<int>(kSampleRate / 2);
        const int measure = static_cast<int>(kSampleRate / 4);
        float peak = 0.0f;
        for (int n = 0; n < settle + measure; ++n) {
            f.Process(level * std::sin(kTwoPi * hz * n / kSampleRate));
            const float y = output == LOW ? f.Low() : output == HIGH ? f.High() : f.Band();
            if (n >= settle) peak = std::fmax(peak, std::fabs(y));
        }
        return Db(peak / level);
    }
}

TEST_CASE("StateVariableFilter high-pass passes the top and cuts the bottom")
{
    // The Digital Heat setting: resonance 0.4
    CHECK(std::fabs(SineGainDb(1000.0f, 0.4f, HIGH, 10000.0f)) < 1.0f);
    CHECK(SineGainDb(1000.0f, 0.4f, HIGH, 1000.0f) > 6.0f);   // resonant peak
    CHECK(SineGainDb(1000.0f, 0.4f, HIGH, 250.0f) < -18.0f);  // 2 octaves down
    CHECK(SineGainDb(1000.0f, 0.4f, HIGH, 100.0f) < -34.0f);  // 12 dB/octave
}

TEST_CASE("StateVariableFilter drive tames the resonance on louder input")
{
    const float quiet = SineGainDb(1000.0f, 0.4f, HIGH, 1000.0f, 0.01f);
    const float loud  = SineGainDb(1000.0f, 0.4f, HIGH, 1000.0f, 0.25f);
    CHECK(loud < quiet - 2.0f);
    CHECK(loud > 0.0f);
}

TEST_CASE("StateVariableFilter low-pass and band-pass")
{
    CHECK(std::fabs(SineGainDb(1000.0f, 0.0f, LOW, 100.0f)) < 1.0f);
    CHECK(SineGainDb(1000.0f, 0.0f, LOW, 10000.0f) < -34.0f);
    // Band-pass peaks at the cutoff
    const float at_cutoff = SineGainDb(1000.0f, 0.4f, BAND, 1000.0f);
    CHECK(at_cutoff > SineGainDb(1000.0f, 0.4f, BAND, 250.0f) + 10.0f);
    CHECK(at_cutoff > SineGainDb(1000.0f, 0.4f, BAND, 4000.0f) + 10.0f);
}

TEST_CASE("StateVariableFilter cutoff tops out at a third of the sample rate")
{
    StateVariableFilter capped, top;
    capped.Init(kSampleRate);
    top.Init(kSampleRate);
    capped.SetRes(0.4f);
    top.SetRes(0.4f);
    capped.SetFreq(20000.0f);
    top.SetFreq(kSampleRate / 3.0f);

    uint32_t state = 1;
    for (int n = 0; n < 4800; ++n) {
        state = state * 1664525u + 1013904223u;
        const float x = static_cast<float>(state >> 8) / static_cast<float>(1u << 24) - 0.5f;
        capped.Process(x);
        top.Process(x);
        REQUIRE(capped.High() == top.High());
    }
}

TEST_CASE("Bitcrush at 16 bits stays within half a 16-bit step")
{
    Bitcrush crush;
    crush.SetBits(16.0f);
    for (int i = -1000; i <= 1000; ++i) {
        const float x = 0.000731f * static_cast<float>(i);
        CHECK(std::fabs(crush.Process(x) - x) <= 0.5f / 32768.0f + 1e-9f);
    }
}

TEST_CASE("Bitcrush rounds to its steps")
{
    Bitcrush crush;
    // 2 bits: steps of 1/2
    crush.SetBits(2.0f);
    CHECK(crush.Process(0.2f) == 0.0f);
    CHECK(crush.Process(0.3f) == 0.5f);
    CHECK(crush.Process(-0.8f) == -1.0f);

    // Fractional depth: steps of 2^-(bits-1), error at most half a step
    crush.SetBits(3.5f);
    const float step = std::pow(2.0f, -2.5f);
    for (int i = -100; i <= 100; ++i) {
        const float x = 0.0097f * static_cast<float>(i);
        const float y = crush.Process(x);
        CHECK(std::fabs(y - x) <= 0.5f * step + 1e-6f);
        const float steps = y / step;
        CHECK(std::fabs(steps - std::round(steps)) < 1e-4f);
    }
}

TEST_CASE("Bitcrush stereo matches mono")
{
    Bitcrush crush;
    crush.SetBits(5.3f);
    float left = 0.123f, right = -0.456f;
    crush.Process(left, right);
    CHECK(left == crush.Process(0.123f));
    CHECK(right == crush.Process(-0.456f));
}
