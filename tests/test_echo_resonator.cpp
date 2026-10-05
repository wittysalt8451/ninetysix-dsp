#include "doctest/doctest.h"

#include "effects/delay/Echo.h"
#include "effects/filters/Resonator.h"

#include <cmath>
#include <cstdint>
#include <vector>

using ninetysix::Echo;
using ninetysix::Resonator;

namespace {
    constexpr float  kSampleRate = 48000.0f;
    constexpr size_t kEchoSize   = 48000;
    float echo_l[kEchoSize], echo_r[kEchoSize];

    // Deterministic noise in [-0.5, 0.5]
    float NextNoise(uint32_t& state) {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state >> 8) / static_cast<float>(1u << 24) - 0.5f;
    }

    float Db(float ratio) { return 20.0f * std::log10(ratio); }

    // Loudest |x| in [from, to)
    float Peak(const std::vector<float>& x, size_t from, size_t to) {
        float p = 0.0f;
        for (size_t i = from; i < to && i < x.size(); ++i) p = std::fmax(p, std::fabs(x[i]));
        return p;
    }

    // Echo fed `in` on both channels; returns the left output
    std::vector<float> RunEcho(Echo& e, const std::vector<float>& in) {
        std::vector<float> out(in.size());
        for (size_t n = 0; n < in.size(); ++n) {
            float l = in[n], r = in[n];
            e.Process(l, r);
            out[n] = l;
        }
        return out;
    }
}

TEST_CASE("Echo repeats on the delay and fades out")
{
    using doctest::Approx;
    Echo e;
    e.Init(kSampleRate, echo_l, echo_r, kEchoSize);
    e.SetDelaySamples(1000);
    e.SetAmount(1.0f);
    e.SetInput(true);

    // A short 1 kHz burst (an impulse would mostly measure how the loop's
    // high cut smears a click, not how the echo decays)
    const size_t start = 4800; // let the parameter smoothing settle
    const size_t burst = 96;
    std::vector<float> in(start + 8000, 0.0f);
    for (size_t i = 0; i < burst; ++i) {
        const float w = 0.5f - 0.5f * std::cos(6.28318530718f * i / burst);
        in[start + i] = w * std::sin(6.28318530718f * 1000.0f * i / kSampleRate);
    }
    const std::vector<float> out = RunEcho(e, in);

    // Dry at unity; the first repeat is the burst itself, one delay later
    for (size_t i = 0; i < burst; ++i) {
        REQUIRE(out[start + i] == Approx(in[start + i]));
        REQUIRE(out[start + 1000 + i] == Approx(in[start + i]).epsilon(0.01));
    }
    // Every later repeat sits on the delay grid and is quieter than the one
    // before; in between there is next to nothing
    float previous = Peak(out, start + 1000, start + 1000 + burst);
    for (size_t k = 2; k <= 6; ++k) {
        const size_t at = start + k * 1000;
        const float on_beat = Peak(out, at, at + burst + 8);
        CHECK(on_beat < previous);
        CHECK(on_beat > 0.15f * previous);
        CHECK(Peak(out, at - 880, at - 20) < 0.02f);
        previous = on_beat;
    }
}

TEST_CASE("Echo out: input off lets the tail fade without taking new audio")
{
    Echo e;
    e.Init(kSampleRate, echo_l, echo_r, kEchoSize);
    e.SetDelaySamples(12000); // one beat at 240 BPM
    e.SetAmount(1.0f);
    e.SetInput(true);

    uint32_t seed = 7;
    std::vector<float> noise(48000);
    for (float& s : noise) s = NextNoise(seed);
    RunEcho(e, noise);

    // Fader cut: the repeats keep going, in tempo, then die out
    e.SetInput(false);
    std::vector<float> silence(12000 * 40, 0.0f);
    const std::vector<float> tail = RunEcho(e, silence);
    CHECK(Peak(tail, 0, 12000) > 0.1f);
    CHECK(Peak(tail, tail.size() - 12000, tail.size()) < 0.001f);
    CHECK_FALSE(e.IsRinging());

    // New input passes dry but never enters the echo
    std::vector<float> impulse(30000, 0.0f);
    impulse[0] = 1.0f;
    const std::vector<float> out = RunEcho(e, impulse);
    CHECK(out[0] == doctest::Approx(1.0f));
    CHECK(Peak(out, 1, out.size()) < 0.0001f);
}

TEST_CASE("Echo tempo change crossfades without a click")
{
    Echo e;
    e.Init(kSampleRate, echo_l, echo_r, kEchoSize);
    e.SetDelaySamples(24000);
    e.SetAmount(0.6f);
    e.SetInput(true);

    std::vector<float> sine(48000 * 3);
    for (size_t n = 0; n < sine.size(); ++n) {
        sine[n] = 0.5f * std::sin(6.28318530718f * 220.0f * static_cast<float>(n) / kSampleRate);
    }
    std::vector<float> first(sine.begin(), sine.begin() + 96000);
    std::vector<float> second(sine.begin() + 96000, sine.end());
    const std::vector<float> before = RunEcho(e, first);
    e.SetDelaySamples(22500); // 120 -> 128 BPM
    const std::vector<float> after = RunEcho(e, second);

    auto max_step = [](const std::vector<float>& x, size_t from, size_t to) {
        float m = 0.0f;
        for (size_t i = from + 1; i < to; ++i) m = std::fmax(m, std::fabs(x[i] - x[i - 1]));
        return m;
    };
    // Sample-to-sample steps across the change stay within the steady
    // state's: a jump between the read heads would stick out
    const float steady = max_step(before, 48000, 96000);
    CHECK(max_step(after, 0, 4800) <= steady * 1.05f);
}

TEST_CASE("Echo at zero amount passes the input untouched")
{
    Echo e;
    e.Init(kSampleRate, echo_l, echo_r, kEchoSize);
    e.SetDelaySamples(1000);
    e.SetInput(true);
    uint32_t seed = 3;
    for (int n = 0; n < 10000; ++n) {
        const float x = NextNoise(seed);
        float l = x, r = -x;
        e.Process(l, r);
        REQUIRE(l == x);
        REQUIRE(r == -x);
    }
}

TEST_CASE("Resonator rings at the set pitch")
{
    for (float hz : {110.0f, 200.0f, 440.0f, 1000.0f}) {
        Resonator res;
        res.Init(kSampleRate);
        res.SetFreq(hz);
        res.SetMix(1.0f);
        res.Init(kSampleRate); // start on pitch and fully wet

        uint32_t seed = 11;
        std::vector<float> wet;
        for (int n = 0; n < 96000; ++n) {
            const float x = NextNoise(seed);
            float l = x, r = x;
            res.Process(l, r);
            if (n >= 48000) wet.push_back(l - x);
        }

        // The wet's autocorrelation peaks at one period
        const float period = kSampleRate / hz;
        size_t best = 0;
        double best_corr = -1e30;
        for (size_t lag = static_cast<size_t>(period * 0.6f);
             lag <= static_cast<size_t>(period * 1.4f); ++lag) {
            double c = 0.0;
            for (size_t i = lag; i < wet.size(); ++i) c += wet[i] * wet[i - lag];
            if (c > best_corr) { best_corr = c; best = lag; }
        }
        INFO("freq " << hz);
        CHECK(std::fabs(static_cast<float>(best) - period) <= 1.0f);
    }
}

TEST_CASE("Resonator stays stable and about as loud across its range")
{
    // Low-passed noise as a stand-in for music: most energy in the lows
    for (float hz : {55.0f, 220.0f, 880.0f, 1800.0f}) {
        Resonator res;
        res.SetFreq(hz);
        res.SetMix(1.0f);
        res.Init(kSampleRate);

        uint32_t seed = 5;
        float lp = 0.0f;
        const float coef = 1.0f - std::exp(-6.28318530718f * 1000.0f / kSampleRate);
        double in_sq = 0.0, wet_sq = 0.0;
        for (int n = 0; n < 48000 * 4; ++n) {
            lp += coef * (NextNoise(seed) - lp);
            const float x = 3.0f * lp;
            float l = x, r = x;
            res.Process(l, r);
            REQUIRE(std::isfinite(l));
            REQUIRE(std::isfinite(r));
            if (n >= 48000) {
                in_sq += x * x;
                wet_sq += (l - x) * (l - x);
            }
        }
        INFO("freq " << hz);
        const float ratio_db = Db(static_cast<float>(std::sqrt(wet_sq / in_sq)));
        CHECK(ratio_db > -4.0f);
        CHECK(ratio_db < 4.0f);
    }
}

TEST_CASE("Resonator rings out in about the set time")
{
    // 0.5 s ring time = 120 dB/s, so 30 dB between 0.1 s and 0.35 s
    for (float hz : {110.0f, 440.0f, 1800.0f}) {
        Resonator res;
        res.SetFreq(hz);
        res.SetMix(1.0f);
        res.SetRingTime(0.5f);
        res.Init(kSampleRate);

        uint32_t seed = 1;
        std::vector<float> wet;
        for (int n = 0; n < 24000; ++n) {
            const float x = n < 480 ? NextNoise(seed) : 0.0f; // 10 ms burst
            float l = x, r = x;
            res.Process(l, r);
            wet.push_back(l - x);
        }
        auto rms_db = [&](size_t from) {
            double sq = 0.0;
            for (size_t i = from; i < from + 1200; ++i) sq += wet[i] * wet[i];
            return 10.0f * std::log10(static_cast<float>(sq / 1200.0));
        };
        INFO("freq " << hz);
        const float drop = rms_db(4800) - rms_db(16800);
        CHECK(drop > 25.0f);
        CHECK(drop < 45.0f);
    }
}

TEST_CASE("Resonator at zero mix passes the input untouched")
{
    Resonator res;
    res.Init(kSampleRate);
    res.SetFreq(300.0f);
    uint32_t seed = 9;
    for (int n = 0; n < 10000; ++n) {
        const float x = NextNoise(seed);
        float l = x, r = -x;
        res.Process(l, r);
        REQUIRE(l == x);
        REQUIRE(r == -x);
    }
}
