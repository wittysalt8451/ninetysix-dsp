#include "doctest/doctest.h"

#include "utils/DelayLine.h"
#include "utils/Lfo.h"
#include "effects/modulation/StereoChorus.h"
#include "effects/modulation/StereoPhaser.h"
#include "effects/filters/Biquad.h"
#include "effects/dynamics/Limiter.h"

#include <cmath>

using namespace ninetysix;

TEST_CASE("DelayLine delays by the configured amount")
{
    using doctest::Approx;
    DelayLine<64> dl;
    dl.Init();
    dl.SetDelay(10.0f);

    // Impulse comes out exactly 10 samples later (read-before-write order)
    for (int i = 0; i < 30; i++) {
        const float expected = i == 10 ? 1.0f : 0.0f;
        CHECK(dl.Read() == Approx(expected));
        dl.Write(i == 0 ? 1.0f : 0.0f);
    }
}

TEST_CASE("DelayLine interpolates fractional delays")
{
    using doctest::Approx;
    DelayLine<64> dl;
    dl.Init();
    dl.SetDelay(5.5f);

    float peak = 0.0f;
    for (int i = 0; i < 20; i++) {
        const float out = dl.Read();
        if (out > peak) peak = out;
        dl.Write(i == 0 ? 1.0f : 0.0f);
    }
    // Impulse energy is split between two adjacent samples
    CHECK(peak == Approx(0.5f));
}

TEST_CASE("Lfo matches the DaisySP waveform conventions")
{
    using doctest::Approx;
    Lfo lfo;
    lfo.Init(48000.0f);
    lfo.SetFreq(480.0f); // 100-sample period

    // Sine starts at 0, peaks at a quarter period
    CHECK(lfo.ProcessSine() == Approx(0.0f));
    for (int i = 0; i < 24; i++) lfo.ProcessSine();
    CHECK(lfo.ProcessSine() == Approx(1.0f).epsilon(0.001));

    // Triangle starts at +1 (DaisySP WAVE_TRI), hits -1 at half period
    lfo.Init(48000.0f);
    lfo.SetFreq(480.0f);
    CHECK(lfo.ProcessTriangle() == Approx(1.0f));
    for (int i = 0; i < 49; i++) lfo.ProcessTriangle();
    CHECK(lfo.ProcessTriangle() == Approx(-1.0f).epsilon(0.001));
}

TEST_CASE("StereoChorus is dry at mix 0 and bounded at full intensity")
{
    StereoChorus chorus;
    chorus.Init(48000.0f);
    chorus.SetMix(0.0f);

    using doctest::Approx;
    for (int i = 0; i < 1000; i++) {
        const float in = sinf(2.0f * 3.14159265f * 440.0f * i / 48000.0f) * 0.5f;
        CHECK(chorus.ProcessLeft(in) == Approx(in));
        CHECK(chorus.ProcessRight(in) == Approx(in));
    }

    chorus.SetIntensity(1.0f);
    for (int i = 0; i < 48000; i++) {
        const float in = sinf(2.0f * 3.14159265f * 220.0f * i / 48000.0f) * 0.5f;
        const float l = chorus.ProcessLeft(in);
        const float r = chorus.ProcessRight(in);
        REQUIRE(std::isfinite(l));
        REQUIRE(std::isfinite(r));
        REQUIRE(fabsf(l) < 2.0f);
        REQUIRE(fabsf(r) < 2.0f);
    }
}

TEST_CASE("StereoPhaser passes through without a buffer and stays bounded with one")
{
    using doctest::Approx;
    StereoPhaser phaser;
    phaser.Init(48000.0f);
    CHECK(phaser.ProcessLeft(0.3f) == Approx(0.3f));

    static float buffer[4096];
    phaser.SetDelays(buffer, 4096);
    phaser.SetMix(0.5f);
    phaser.SetFeedback(0.5f);
    for (int i = 0; i < 48000; i++) {
        const float in = sinf(2.0f * 3.14159265f * 330.0f * i / 48000.0f) * 0.5f;
        const float l = phaser.ProcessLeft(in);
        const float r = phaser.ProcessRight(in);
        REQUIRE(std::isfinite(l));
        REQUIRE(std::isfinite(r));
        REQUIRE(fabsf(l) < 4.0f);
        REQUIRE(fabsf(r) < 4.0f);
    }
}

TEST_CASE("Biquad lowpass passes DC and attenuates high frequencies")
{
    Biquad lp;
    lp.Init(48000.0f, Biquad::LOWPASS);
    lp.SetFreq(1000.0f);

    float dc = 0.0f;
    for (int i = 0; i < 2000; i++) dc = lp.Process(1.0f);
    CHECK(dc == doctest::Approx(1.0f).epsilon(0.01));

    lp.Init(48000.0f, Biquad::LOWPASS);
    lp.SetFreq(1000.0f);
    float peak = 0.0f;
    for (int i = 0; i < 4800; i++) {
        const float in = sinf(2.0f * 3.14159265f * 12000.0f * i / 48000.0f);
        const float out = fabsf(lp.Process(in));
        if (i > 1000 && out > peak) peak = out;
    }
    CHECK(peak < 0.05f);
}

TEST_CASE("Limiter keeps peaks near the threshold")
{
    Limiter lim;
    lim.Init(0.5f, 0.5f, 0.01f);

    float peak = 0.0f;
    for (int i = 0; i < 48000; i++) {
        const float in = sinf(2.0f * 3.14159265f * 100.0f * i / 48000.0f) * 2.0f;
        const float out = fabsf(lim.Process(in));
        if (i > 4800 && out > peak) peak = out;
    }
    CHECK(peak < 0.75f);
    CHECK(peak > 0.25f);
}
