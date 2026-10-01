#include "doctest/doctest.h"

#include "effects/glitch/Stutter.h"

#include <cstdint>
#include <vector>

using ninetysix::Stutter;

namespace {
    constexpr uint32_t kSampleRate = 48000;
    constexpr uint32_t kBlock      = 4;
    constexpr size_t   kRingSize   = kSampleRate * 16;
    constexpr size_t   kCapSize    = kSampleRate * 8 + Stutter::kXfadeSamples;
    constexpr uint32_t kXfade      = Stutter::kXfadeSamples;

    float ring_l[kRingSize], ring_r[kRingSize];
    float cap_l[kCapSize], cap_r[kCapSize];

    // Amounts per knob zone: 0.5 -> 1/16 bar, 0.4 -> 1/8 bar, 0.05 -> 1 bar
    constexpr float kSixteenth = 0.5f;
    constexpr float kEighth    = 0.4f;
    constexpr float kWholeBar  = 0.05f;

    // Input is a timestamp (sample index), so every output sample tells
    // exactly which input sample is playing. Right is the negated left.
    float In(uint64_t t) { return static_cast<float>(t); }

    enum class Fx { kRoll, kReverse };

    // Drives a Stutter like the Patch SM audio callback: 4-sample blocks,
    // one clock pulse per beat seen at the start of a block, the first
    // pulse taken as the downbeat
    struct Rig {
        Stutter  st;
        uint32_t beat;
        bool     clock = true;
        uint64_t t = 0;
        uint64_t next_pulse = 0;
        uint32_t pulses = 0;
        std::vector<float> out_l, out_r;

        explicit Rig(uint32_t beat_samples) : beat(beat_samples) {
            st.Init(static_cast<float>(kSampleRate));
            st.SetBuffers(ring_l, ring_r, kRingSize, cap_l, cap_r, kCapSize);
            st.SetQuantize(true);
        }

        uint32_t Bar() const { return 4 * beat; }

        void Run(uint64_t until, Fx fx) {
            for (; t < until; t += kBlock) {
                if (clock && t >= next_pulse) {
                    if (pulses++ == 0) {
                        st.SyncDownbeat();
                    } else {
                        st.SetBeatSamples(beat);
                        st.SyncBeat();
                    }
                    next_pulse += beat;
                }
                for (uint32_t i = 0; i < kBlock; ++i) {
                    float l = In(t + i), r = -In(t + i);
                    if (fx == Fx::kRoll) st.Process(l, r);
                    else st.ProcessReverse(l, r);
                    out_l.push_back(l);
                    out_r.push_back(r);
                }
            }
        }

        // Output equals input sample `src` on both channels
        bool Plays(uint64_t at, uint64_t src) const {
            return out_l[at] == In(src) && out_r[at] == -In(src);
        }
    };
}

TEST_CASE("Stutter reverse waits for the bar line and mirrors around it")
{
    Rig rig(23040); // 125 BPM
    const uint64_t bar = rig.Bar();

    rig.Run(2 * bar + 31000, Fx::kReverse); // knob opens 1.3 beats into bar 3
    rig.st.SetAmount(0.5f);
    const uint64_t line = 3 * bar;
    rig.Run(line + 2 * bar, Fx::kReverse);

    // Dry until the line, however long the wait
    for (uint64_t t = 0; t < line; ++t) REQUIRE(rig.Plays(t, t));

    // From the line: the bar before it, backwards. Sample line+d plays
    // line-1-d, so every hit of that bar lands back on a beat. Never
    // anything the ring has not recorded yet.
    for (uint64_t d = 0; d < bar - kXfade; ++d) {
        REQUIRE(rig.Plays(line + d, line - 1 - d));
        REQUIRE(rig.Plays(line + bar + d, line - 1 - d)); // and it loops
    }
}

TEST_CASE("Stutter reverse releases on the bar line with the downbeat dry")
{
    Rig rig(23040);
    const uint64_t bar = rig.Bar();

    rig.Run(bar + 5000, Fx::kReverse);
    rig.st.SetAmount(0.5f);
    rig.Run(3 * bar + 50000, Fx::kReverse); // engaged on 2 * bar
    rig.st.SetAmount(0.0f);                 // closed halfway bar 4
    const uint64_t release = 4 * bar;
    rig.Run(release + bar, Fx::kReverse);

    // Still reversed until the fade that ends exactly on the line
    for (uint64_t t = 3 * bar; t < release - kXfade; ++t) {
        REQUIRE(rig.Plays(t, 2 * bar - 1 - (t - 3 * bar)));
    }
    // The downbeat and everything after it is the untouched input
    for (uint64_t t = release; t < release + bar; ++t) REQUIRE(rig.Plays(t, t));
    CHECK_FALSE(rig.st.IsActive());
}

TEST_CASE("Stutter roll captures from the bar line and repeats on the grid")
{
    Rig rig(22500); // 128 BPM: slices of 351/352 samples at 1/256 bar
    const uint64_t bar = rig.Bar();

    rig.Run(bar + 12345, Fx::kRoll);
    rig.st.SetAmount(kSixteenth);
    const uint64_t line = 2 * bar;
    const uint64_t slice = bar / 16;
    rig.Run(line + 3 * bar, Fx::kRoll);

    for (uint64_t t = 0; t < line; ++t) REQUIRE(rig.Plays(t, t));
    // First pass is the live input (the slice is being recorded), then the
    // slice repeats in phase with the grid; skip the seam crossfades
    for (uint64_t d = 0; d < 3 * bar; ++d) {
        if (d % slice >= slice - kXfade) continue;
        REQUIRE(rig.Plays(line + d, line + d % slice));
    }
}

TEST_CASE("Stutter roll division changes stay on the grid")
{
    Rig rig(23040);
    const uint64_t bar = rig.Bar();

    rig.Run(bar + 5000, Fx::kRoll);
    rig.st.SetAmount(kSixteenth);
    const uint64_t line = 2 * bar;
    rig.Run(line + 20000, Fx::kRoll);
    rig.st.SetAmount(kEighth);
    rig.Run(4 * bar, Fx::kRoll);

    const uint64_t slice = bar / 8;
    for (uint64_t t = line + 20000 + kXfade; t < 4 * bar; ++t) {
        const uint64_t d = t - line;
        if (d % slice >= slice - kXfade) continue;
        REQUIRE(rig.Plays(t, line + d % slice));
    }
}

TEST_CASE("Stutter roll re-engaged right after a release plays fresh audio")
{
    // The ring keeps recording while an effect plays, so the next capture
    // is never stale or shifted
    Rig rig(23040);
    const uint64_t bar = rig.Bar();
    const uint64_t slice = bar / 16;

    rig.st.SetAmount(kSixteenth);
    rig.Run(bar + 30000, Fx::kRoll);  // engaged on the first downbeat
    rig.st.SetAmount(0.0f);
    rig.Run(2 * bar - 50, Fx::kRoll); // release fade is under way
    rig.st.SetAmount(kSixteenth);     // reopened just before the line
    rig.Run(5 * bar, Fx::kRoll);

    // Released and re-engaged on the same line, with a new capture
    for (uint64_t d = 0; d < 3 * bar; ++d) {
        if (d % slice >= slice - kXfade) continue;
        REQUIRE(rig.Plays(2 * bar + d, 2 * bar + d % slice));
    }
}

TEST_CASE("Stutter effects can be held longer than the ring")
{
    Rig rig(23040);
    const uint64_t bar = rig.Bar(); // 12 bars = 23 s > 16 s ring

    SUBCASE("roll") {
        rig.Run(bar + 100, Fx::kRoll);
        rig.st.SetAmount(kWholeBar);
        rig.Run(14 * bar, Fx::kRoll);
        for (uint64_t d = 0; d < 12 * bar; ++d) {
            if (d % bar >= bar - kXfade) continue;
            REQUIRE(rig.Plays(2 * bar + d, 2 * bar + d % bar));
        }
    }
    SUBCASE("reverse") {
        rig.Run(bar + 100, Fx::kReverse);
        rig.st.SetAmount(0.5f);
        rig.Run(14 * bar, Fx::kReverse);
        for (uint64_t d = 0; d < 12 * bar; ++d) {
            if (d % bar >= bar - kXfade) continue;
            REQUIRE(rig.Plays(2 * bar + d, 2 * bar - 1 - d % bar));
        }
    }
}

TEST_CASE("Stutter without a clock switches immediately")
{
    Rig rig(23040);
    rig.clock = false;
    rig.st.SetQuantize(false);

    rig.Run(70000, Fx::kRoll);
    rig.st.SetAmount(kSixteenth);
    rig.Run(70000 + 40000, Fx::kRoll);
    rig.st.SetAmount(0.0f);
    rig.Run(70000 + 60000, Fx::kRoll);

    // The bar restarts at the engage point; the default tempo is 120 BPM
    const uint64_t slice = 4 * 24000 / 16;
    for (uint64_t d = 0; d < 40000; ++d) {
        if (d % slice >= slice - kXfade) continue;
        REQUIRE(rig.Plays(70000 + d, 70000 + d % slice));
    }
    for (uint64_t t = 70000 + 40000 + kXfade; t < 70000 + 60000; ++t) {
        REQUIRE(rig.Plays(t, t));
    }
}

TEST_CASE("Stutter grid counts beats through missed and doubled pulses")
{
    Stutter st;
    st.Init(static_cast<float>(kSampleRate));
    st.SetBuffers(ring_l, ring_r, kRingSize, cap_l, cap_r, kCapSize);
    const uint32_t beat = 20000;
    st.SetBeatSamples(beat);

    auto advance = [&](uint32_t n) {
        for (uint32_t i = 0; i < n; ++i) st.Record(0.0f, 0.0f);
    };

    st.SyncDownbeat();
    CHECK(st.GetGridPos() == 0);
    advance(beat + 8); // pulse arrives late
    st.SyncBeat();
    CHECK(st.GetGridPos() == beat);
    advance(12);       // double trigger: ignored
    st.SyncBeat();
    CHECK(st.GetGridPos() == beat + 12);
    advance(2 * beat - 12); // one pulse missed
    st.SyncBeat();
    CHECK(st.GetGridPos() == 3 * beat);
    advance(beat - 4);      // pulse slightly early: the next bar starts
    st.SyncBeat();
    CHECK(st.GetGridPos() == 0);
}

TEST_CASE("Stutter grid survives a large tempo drop")
{
    // Rounding the free-running grid put beat 2 on beat 1 when the tempo
    // fell to a quarter; counting beats does not
    Stutter st;
    st.Init(static_cast<float>(kSampleRate)); // free-runs at 120 BPM
    st.SetBuffers(ring_l, ring_r, kRingSize, cap_l, cap_r, kCapSize);
    st.SyncDownbeat();
    for (uint32_t i = 0; i < 96000; ++i) st.Record(0.0f, 0.0f); // 30 BPM beat
    st.SetBeatSamples(96000);
    st.SyncBeat();
    CHECK(st.GetGridPos() == 96000);
}
