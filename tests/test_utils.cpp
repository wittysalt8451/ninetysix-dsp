#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "utils/Mapping.h"
#include "utils/Tempo.h"
#include "utils/Slew.h"
#include "utils/EnvelopeFollower.h"
#include "envelopes/Envelope.h"

using namespace ninetysix;

TEST_CASE("MapLinear clamps and interpolates")
{
    using doctest::Approx;
    CHECK(MapLinear(0.f, 0.f, 100.f) == Approx(0.f));
    CHECK(MapLinear(1.f, 0.f, 100.f) == Approx(100.f));
    CHECK(MapLinear(0.5f, 10.f, 20.f) == Approx(15.f));
    CHECK(MapLinear(-0.5f, 0.f, 10.f) == Approx(0.f));
    CHECK(MapLinear(1.5f, 0.f, 10.f) == Approx(10.f));
}

TEST_CASE("MapLogarithmic endpoints and midpoint")
{
    using doctest::Approx;
    CHECK(MapLogarithmic(0.f, 1.f, 1000.f) == Approx(1.f));
    CHECK(MapLogarithmic(1.f, 1.f, 1000.f) == Approx(1000.f));
    // Geometric mean of 1 and 100 at t=0.5
    CHECK(MapLogarithmic(0.5f, 1.f, 100.f) == Approx(10.f));
}

TEST_CASE("CalculateReleaseTime")
{
    using doctest::Approx;
    CHECK(CalculateReleaseTime(0.f) == Approx(0.01f));
    CHECK(CalculateReleaseTime(-10.f) == Approx(0.01f));
    // 120 BPM: beat = 0.5 s, division 4 -> 0.125 s
    CHECK(CalculateReleaseTime(120.f, 0.01f, 1.f, 4.f) == Approx(0.125f));
    CHECK(CalculateReleaseTime(60.f, 0.01f, 10.f, 1.f) == Approx(1.f));
}

TEST_CASE("SlewTowards")
{
    using doctest::Approx;
    CHECK(SlewTowards(10.f, 0.f, 0.5f) == Approx(5.f));
    CHECK(SlewTowards(0.f, 10.f, 0.1f) == Approx(9.f));
    CHECK(SlewTowards(1.f, 1.f, 0.3f) == Approx(1.f));
}

TEST_CASE("EnvelopeFollower smoothing")
{
    using doctest::Approx;
    EnvelopeFollower fast;
    fast.Init(0.f);
    CHECK(fast.Process(1.f, -1.f) == Approx(1.f));

    EnvelopeFollower slow;
    slow.Init(0.99f);
    CHECK(slow.Process(0.f, 0.f) == Approx(0.f));
    float y = slow.Process(1.f, 1.f);
    CHECK(y > 0.f);
    CHECK(y < 1.f);
    for (int i = 0; i < 2000; ++i) {
        y = slow.Process(1.f, 1.f);
    }
    CHECK(y == Approx(1.f).epsilon(1e-4f));
}

TEST_CASE("Envelope ADSR attack step")
{
    using doctest::Approx;
    Envelope env;
    // attackTime * sampleRate = 1 -> one Process() reaches peak from 0
    env.Init(1.0f / 100.f, 0.2f, 0.5f, 0.2f, 100.f);
    CHECK(env.Process() == Approx(0.f));
    env.Trigger();
    CHECK(env.Process() == Approx(1.f));
}
