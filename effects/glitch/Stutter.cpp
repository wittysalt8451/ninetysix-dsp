#include "glitch/Stutter.h"

using namespace ninetysix;

namespace {
    constexpr float    kActiveThreshold = 0.01f;
    constexpr uint32_t kBeatsPerBar     = 4;
    constexpr uint32_t kMinBeatSamples  = 64;
    // Powers of two from 1 bar down to 1/256 bar (= 1/64 beat)
    constexpr int      kNumDivisions    = 9;
    constexpr uint32_t kMinSliceSamples = 16;
    // The reverse moves its bar out of the ring this many samples per
    // sample: done within a quarter bar, long before the write head comes
    // round to it
    constexpr uint32_t kCopyPerSample   = 4;

    inline float Mix(float a, float b, float t) { return a + (b - a) * t; }

    // Seam crossfade, shortened for very short slices
    inline uint32_t SeamLen(uint32_t len) {
        const uint32_t f = Stutter::kXfadeSamples;
        return len < 4 * f ? len / 4 + 1 : f;
    }
}

void Stutter::Init(float sample_rate) {
    sample_rate_  = sample_rate;
    beat_samples_ = static_cast<uint32_t>(sample_rate * 0.5f); // 120 BPM until clocked
    grid_pos_ = sync_beat_ = since_sync_ = 0;
    bar_line_ = quantize_ = false;
    write_pos_ = 0;
    active_ = releasing_ = false;
    amount_ = 0.0f;
}

void Stutter::SetBuffers(float* ring_left, float* ring_right, size_t ring_size,
                         float* cap_left, float* cap_right, size_t cap_size) {
    ring_l_ = ring_left;
    ring_r_ = ring_right;
    ring_size_ = ring_size;
    cap_l_ = cap_left;
    cap_r_ = cap_right;
    cap_size_ = cap_size;
    for (size_t i = 0; i < ring_size; ++i) {
        ring_left[i]  = 0.0f;
        ring_right[i] = 0.0f;
    }
    for (size_t i = 0; i < cap_size; ++i) {
        cap_left[i]  = 0.0f;
        cap_right[i] = 0.0f;
    }
    ready_ = ring_left && ring_right && cap_left && cap_right
             && ring_size >= 2 * kBeatsPerBar * kMinBeatSamples
             && cap_size >= kBeatsPerBar * kMinBeatSamples + kXfadeSamples;
    write_pos_ = 0;
    active_ = false;
    SetBeatSamples(beat_samples_); // re-clamp the tempo to the new memory
}

void Stutter::SetAmount(float amount) {
    amount_ = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
}

void Stutter::SetBeatSamples(uint32_t samples) {
    // The ring must hold two bars (the reverse fetches a whole bar from
    // it), the capture buffer one bar plus the seam crossfade
    if (ready_) {
        size_t max_beat = ring_size_ / (2 * kBeatsPerBar);
        const size_t max_cap = (cap_size_ - kXfadeSamples) / kBeatsPerBar;
        if (max_cap < max_beat) max_beat = max_cap;
        if (samples > max_beat) samples = static_cast<uint32_t>(max_beat);
    }
    if (samples < kMinBeatSamples) samples = kMinBeatSamples;
    beat_samples_ = samples;
    if (grid_pos_ >= BarLen()) grid_pos_ %= BarLen();
}

void Stutter::SyncBeat() {
    // Count whole beats since the last pulse: normally 1, more after missed
    // pulses, 0 for a double trigger (ignored). Counting rather than
    // rounding the free-running grid keeps the bar phase at any tempo jump.
    const uint32_t beats = (since_sync_ + beat_samples_ / 2) / beat_samples_;
    if (beats == 0) return;
    sync_beat_  = (sync_beat_ + beats) % kBeatsPerBar;
    grid_pos_   = sync_beat_ * beat_samples_;
    since_sync_ = 0;
    if (sync_beat_ == 0) bar_line_ = true;
}

void Stutter::SyncDownbeat() {
    sync_beat_  = 0;
    grid_pos_   = 0;
    since_sync_ = 0;
    bar_line_   = true;
}

void Stutter::AdvanceGrid() {
    if (++grid_pos_ >= BarLen()) grid_pos_ = 0;
    if (since_sync_ < UINT32_MAX) ++since_sync_;
}

void Stutter::WriteRing(float left, float right) {
    ring_l_[write_pos_] = left;
    ring_r_[write_pos_] = right;
    if (++write_pos_ >= ring_size_) write_pos_ = 0;
}

// Offsets stay within one ring length of the engage point, so a single
// wrap replaces the (costly on the M7) 64-bit modulo
size_t Stutter::RingIndex(int64_t offset_from_engage) const {
    const int64_t size = static_cast<int64_t>(ring_size_);
    int64_t idx = static_cast<int64_t>(engage_pos_) + offset_from_engage;
    if (idx < 0) idx += size;
    else if (idx >= size) idx -= size;
    return static_cast<size_t>(idx);
}

int Stutter::Division() const {
    int k = static_cast<int>(amount_ * static_cast<float>(kNumDivisions));
    if (k >= kNumDivisions) k = kNumDivisions - 1;
    while (k > 0 && (BarLen() >> k) < kMinSliceSamples) --k;
    return k;
}

void Stutter::Record(float left, float right) {
    if (!ready_) return;
    active_   = false; // another effect took the slot: drop the effect
    bar_line_ = false;
    WriteRing(left, right);
    AdvanceGrid();
}

void Stutter::Process(float& left, float& right) {
    Tick(Mode::kRoll, left, right);
}

void Stutter::ProcessReverse(float& left, float& right) {
    Tick(Mode::kReverse, left, right);
}

void Stutter::Engage(Mode mode, size_t now) {
    if (!quantize_) {
        // No clock: the bar starts right here
        grid_pos_  = 0;
        sync_beat_ = 0;
        since_sync_ = 0;
    }
    active_      = true;
    releasing_   = false;
    mode_        = mode;
    engage_pos_  = now;
    elapsed_     = 0;
    release_pos_ = 0;

    if (mode == Mode::kRoll) {
        // Pre-roll: the audio just before the line, which every slice
        // crossfades into so the next pass starts seamlessly
        for (uint32_t i = 0; i < kXfadeSamples; ++i) {
            const size_t r = RingIndex(static_cast<int64_t>(i) - kXfadeSamples);
            cap_l_[i] = ring_l_[r];
            cap_r_[i] = ring_r_[r];
        }
        cap_len_ = BarLen();
        div_ = old_div_ = Division();
        div_fade_pos_ = kXfadeSamples;
    } else {
        rev_len_  = BarLen();
        copy_pos_ = rev_len_; // nothing copied yet
    }
}

void Stutter::Tick(Mode mode, float& left, float& right) {
    if (!ready_) return;

    const float  in_l = left;
    const float  in_r = right;
    const size_t now  = write_pos_;
    // The ring keeps recording while an effect plays, so it never holds
    // stale or misplaced audio when the next effect engages
    WriteRing(in_l, in_r);

    if (active_ && mode_ != mode) active_ = false; // roll <-> reverse switch

    const bool want = amount_ > kActiveThreshold;
    // With a clock, changes wait for a bar line; without one they apply at once
    const bool line = quantize_ ? bar_line_ : true;
    bar_line_ = false;

    if (active_ && !want && !releasing_) {
        // Time the fade to end on the expected line, so the downbeat plays
        // dry and intact; a line earlier than expected fades from there
        if (line || BarLen() - grid_pos_ <= kXfadeSamples) {
            releasing_   = true;
            release_pos_ = 0;
        }
    }

    if (!active_) {
        if (!want || !line) {
            AdvanceGrid();
            return;
        }
        Engage(mode, now);
    }

    if (mode_ == Mode::kRoll) {
        // Capture the first bar from the line on: the first pass of a
        // slice plays straight from it, so it is the live input
        if (elapsed_ < cap_len_) {
            cap_l_[kXfadeSamples + elapsed_] = in_l;
            cap_r_[kXfadeSamples + elapsed_] = in_r;
        }
    } else {
        // Copy the mirrored bar out of the ring, newest sample first (the
        // order it plays in), plus the audio just after the line for the
        // loop seam
        for (uint32_t n = 0; n < kCopyPerSample && copy_pos_ > 0; ++n) {
            --copy_pos_;
            const size_t r = RingIndex(static_cast<int64_t>(copy_pos_)
                                       - static_cast<int64_t>(rev_len_));
            cap_l_[copy_pos_] = ring_l_[r];
            cap_r_[copy_pos_] = ring_r_[r];
        }
        if (elapsed_ < kXfadeSamples) {
            cap_l_[rev_len_ + elapsed_] = in_l;
            cap_r_[rev_len_ + elapsed_] = in_r;
        }
    }

    float wet_l, wet_r;
    if (mode_ == Mode::kRoll) {
        // The division follows the knob, and holds while it closes
        if (want) {
            const int div = Division();
            if (div != div_) {
                old_div_ = div_;
                div_ = div;
                div_fade_pos_ = 0;
            }
        }
        uint32_t phase, len;
        SliceAt(div_, phase, len);
        wet_l = RollRead(cap_l_, phase, len);
        wet_r = RollRead(cap_r_, phase, len);
        if (div_fade_pos_ < kXfadeSamples) {
            SliceAt(old_div_, phase, len);
            const float t = static_cast<float>(div_fade_pos_++) / kXfadeSamples;
            wet_l = Mix(RollRead(cap_l_, phase, len), wet_l, t);
            wet_r = Mix(RollRead(cap_r_, phase, len), wet_r, t);
        }
    } else {
        wet_l = ReverseRead(cap_l_, ring_l_);
        wet_r = ReverseRead(cap_r_, ring_r_);
    }

    if (releasing_) {
        const float t = static_cast<float>(release_pos_++) / kXfadeSamples;
        left  = Mix(wet_l, in_l, t);
        right = Mix(wet_r, in_r, t);
        if (release_pos_ >= kXfadeSamples) active_ = false;
    } else {
        left  = wet_l;
        right = wet_r;
    }

    if (elapsed_ < UINT32_MAX) ++elapsed_;
    AdvanceGrid();
}

// Slice under the grid position. Its edges come straight from the bar
// grid, so every slice starts on its exact grid position however the bar
// divides.
void Stutter::SliceAt(int division, uint32_t& phase, uint32_t& len) const {
    const uint64_t bar   = BarLen();
    const uint64_t idx   = (static_cast<uint64_t>(grid_pos_) << division) / bar;
    const uint32_t start = static_cast<uint32_t>((idx * bar) >> division);
    len   = static_cast<uint32_t>(((idx + 1) * bar) >> division) - start;
    phase = grid_pos_ - start;
}

// Roll: slot `phase` of every slice plays capture sample `phase`, so the
// repeat is phase-locked to the bar grid
float Stutter::RollRead(const float* cap, uint32_t phase, uint32_t len) const {
    // Until the first pass has recorded a slot, the slot is the live input
    uint32_t src = phase < elapsed_ ? phase : elapsed_;
    if (src >= cap_len_) src = cap_len_ - 1;
    float out = cap[kXfadeSamples + src];

    // Seam: the end of the slice crossfades into the audio just before the
    // capture point, which runs gaplessly into the slice's first sample, so
    // the repeat neither clicks nor softens its attack
    const uint32_t seam = SeamLen(len);
    if (phase + seam >= len) {
        const uint32_t back = len - phase; // 1..seam
        const float t = 1.0f - static_cast<float>(back) / static_cast<float>(seam + 1);
        out = Mix(out, cap[kXfadeSamples - back], t);
    }
    return out;
}

// Reverse: mirror around the engage point. Grid slot q plays the sample
// q + 1 before the line, so every hit of the bar lands back on a beat
float Stutter::ReverseRead(const float* cap, const float* ring) const {
    const uint32_t bar = BarLen();
    const uint32_t q   = grid_pos_; // the loop restarts on every bar line
    const uint32_t x   = q < rev_len_ ? rev_len_ - 1 - q : 0;
    float out = x >= copy_pos_
                    ? cap[x]
                    : ring[RingIndex(static_cast<int64_t>(x) - static_cast<int64_t>(rev_len_))];

    // Seam: the end of the loop crossfades into the audio just after the
    // line, reversed, which leads gaplessly into the loop's first sample
    const uint32_t seam = SeamLen(bar);
    if (q + seam >= bar) {
        const uint32_t back = bar - q; // 1..seam
        uint32_t post = back - 1;      // samples after the line
        if (post > elapsed_) post = elapsed_;
        const float t = 1.0f - static_cast<float>(back) / static_cast<float>(seam + 1);
        out = Mix(out, cap[rev_len_ + post], t);
    }
    return out;
}
