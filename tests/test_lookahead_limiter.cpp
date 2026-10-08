#include "doctest/doctest.h"

#include "effects/dynamics/LookaheadLimiter.h"

#include <cmath>
#include <cstdint>
#include <vector>

using ninetysix::LookaheadLimiter;

namespace {
    constexpr float kSampleRate = 48000.0f;
    constexpr float kTwoPi      = 6.28318530718f;

    // Deterministic noise in [-0.5, 0.5]
    float NextNoise(uint32_t& state) {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state >> 8) / static_cast<float>(1u << 24) - 0.5f;
    }

    float Sine(float level, float hz, int n) {
        return level * std::sin(kTwoPi * hz * n / kSampleRate);
    }

    // Limiter fed `in` on both channels; returns the left output
    std::vector<float> Run(LookaheadLimiter& lim, const std::vector<float>& in) {
        std::vector<float> out(in.size());
        for (size_t n = 0; n < in.size(); ++n) {
            float l = in[n], r = in[n];
            lim.Process(l, r);
            out[n] = l;
        }
        return out;
    }

    // Loudest |x| in [from, to)
    float Peak(const std::vector<float>& x, size_t from, size_t to) {
        float p = 0.0f;
        for (size_t i = from; i < to && i < x.size(); ++i) p = std::fmax(p, std::fabs(x[i]));
        return p;
    }
}

TEST_CASE("LookaheadLimiter latency follows the lookahead")
{
    LookaheadLimiter lim;
    lim.Init(kSampleRate, 1.0f, 150.0f);
    CHECK(lim.Latency() == 47);
    lim.Init(kSampleRate, 100.0f, 150.0f);
    CHECK(lim.Latency() == LookaheadLimiter::kMaxLookahead - 1);
    lim.Init(kSampleRate, 0.0f, 150.0f);
    CHECK(lim.Latency() == 0);
}

TEST_CASE("LookaheadLimiter passes a signal under the ceiling untouched")
{
    LookaheadLimiter lim;
    lim.Init(kSampleRate);
    lim.SetCeiling(0.9f);
    std::vector<float> in(4800);
    for (int n = 0; n < 4800; ++n) in[n] = Sine(0.5f, 440.0f, n);
    const std::vector<float> out = Run(lim, in);
    const int d = lim.Latency();
    float err = 0.0f;
    for (int n = d; n < 4800; ++n) err = std::fmax(err, std::fabs(out[n] - in[n - d]));
    CHECK(err < 1e-6f);
}

TEST_CASE("LookaheadLimiter never lets a sample over the ceiling")
{
    // Noise bursts that jump in level every few ms, driven hard
    for (int lookahead_ms : {0, 1, 5}) {
        for (float drive : {1.0f, 4.0f, 8.0f}) {
            LookaheadLimiter lim;
            lim.Init(kSampleRate, static_cast<float>(lookahead_ms), 150.0f);
            lim.SetCeiling(0.9f);
            lim.SetDrive(drive);
            uint32_t state = 7;
            float level = 0.0f;
            float worst = 0.0f;
            for (int n = 0; n < 48000; ++n) {
                if (n % 150 == 0) level = 3.0f * (NextNoise(state) + 0.5f);
                float l = level * 2.0f * NextNoise(state);
                float r = level * 2.0f * NextNoise(state);
                lim.Process(l, r);
                worst = std::fmax(worst, std::fmax(std::fabs(l), std::fabs(r)));
            }
            CHECK(worst <= 0.9f + 1e-5f);
        }
    }
}

TEST_CASE("LookaheadLimiter scales a loud sine without reshaping it")
{
    // 1 kHz: every 1 ms window holds a peak, so the gain settles flat at
    // ceiling / level and the sine comes out as a clean, smaller sine
    LookaheadLimiter lim;
    lim.Init(kSampleRate);
    lim.SetCeiling(0.5f);
    std::vector<float> in(9600);
    for (int n = 0; n < 9600; ++n) in[n] = Sine(1.0f, 1000.0f, n);
    const std::vector<float> out = Run(lim, in);
    const int d = lim.Latency();
    float err = 0.0f;
    for (int n = 4800; n < 9600; ++n) err = std::fmax(err, std::fabs(out[n] - 0.5f * in[n - d]));
    CHECK(err < 1e-4f);
}

TEST_CASE("LookaheadLimiter gain is down before the peak comes out")
{
    // A quiet signal steps up to a loud one; the samples just before the
    // step already come out softer, and the step itself lands on the ceiling
    LookaheadLimiter lim;
    lim.Init(kSampleRate);
    lim.SetCeiling(0.5f);
    std::vector<float> in(2000, 0.2f);
    for (size_t n = 1000; n < in.size(); ++n) in[n] = 1.0f;
    const std::vector<float> out = Run(lim, in);
    const int d = lim.Latency();
    CHECK(out[1000 + d - 1] < 0.2f * 0.6f);   // glided down ahead
    CHECK(out[1000 + d] <= 0.5f + 1e-6f);
    CHECK(out[1000 + d] > 0.5f - 1e-3f);
}

TEST_CASE("LookaheadLimiter drive pushes the level up to the ceiling")
{
    LookaheadLimiter lim;
    lim.Init(kSampleRate);
    lim.SetCeiling(0.9f);
    lim.SetDrive(4.0f);
    std::vector<float> in(9600);
    for (int n = 0; n < 9600; ++n) in[n] = Sine(0.5f, 1000.0f, n);
    const std::vector<float> out = Run(lim, in);
    const float peak = Peak(out, 4800, 9600);
    CHECK(peak <= 0.9f + 1e-5f);
    CHECK(peak > 0.89f);
}

TEST_CASE("LookaheadLimiter recovers after a peak with the release")
{
    LookaheadLimiter lim;
    lim.Init(kSampleRate, 1.0f, 150.0f);
    lim.SetCeiling(0.5f);
    // A loud bang, then a quiet sine that needs no limiting
    std::vector<float> in(48000);
    for (int n = 0; n < 48000; ++n) in[n] = n < 480 ? Sine(1.0f, 1000.0f, n) : Sine(0.1f, 1000.0f, n);
    const std::vector<float> out = Run(lim, in);
    // Shortly after: still held down
    CHECK(Peak(out, 1200, 1680) < 0.08f);
    // A second later (over six release time constants): back at unity
    CHECK(std::fabs(Peak(out, 43200, 48000) - 0.1f) < 0.001f);
}

TEST_CASE("LookaheadLimiter links the channels")
{
    // A loud left pulls the right down by the same gain
    LookaheadLimiter lim;
    lim.Init(kSampleRate);
    lim.SetCeiling(0.5f);
    float ratio = 0.0f;
    for (int n = 0; n < 9600; ++n) {
        float l = Sine(1.0f, 1000.0f, n);
        float r = Sine(0.2f, 1000.0f, n);
        lim.Process(l, r);
        if (n > 4800 && std::fabs(l) > 0.4f) ratio = r / l;
    }
    CHECK(ratio == doctest::Approx(0.2f).epsilon(1e-4));
}
