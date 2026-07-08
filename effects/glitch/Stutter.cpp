#include "glitch/Stutter.h"

using namespace ninetysix;

namespace {
    constexpr float    kActiveThreshold = 0.01f;
    constexpr uint32_t kMinLoopSamples  = 16;
    // Powers of two from 1 bar down to 1/256 bar (= 1/64 beat)
    constexpr int      kNumDivisions    = 9;
    // ~2.7 ms at 48 kHz: short enough to stay tight, long enough to kill clicks
    constexpr uint32_t kFadeSamples     = 128;

    // Mini fade-in/out at the edges of a repeating span
    float EdgeEnv(uint32_t phase, uint32_t len) {
        uint32_t f = kFadeSamples;
        if (len < 4 * f) f = len / 4 + 1;
        float env = 1.0f;
        if (phase < f) env = static_cast<float>(phase) / static_cast<float>(f);
        const uint32_t tail = len - phase;
        if (tail < f) {
            const float e = static_cast<float>(tail) / static_cast<float>(f);
            if (e < env) env = e;
        }
        return env;
    }

    inline float Mix(float a, float b, float t) { return a + (b - a) * t; }
}

void Stutter::Init(float sample_rate) {
    sample_rate_  = sample_rate;
    beat_samples_ = static_cast<uint32_t>(sample_rate * 0.5f); // 120 BPM until clocked
    write_pos_ = bar_start_pos_ = grid_pos_ = engage_grid_ = 0;
    active_ = releasing_ = false;
    amount_ = 0.0f;
    engage_fade_pos_ = release_fade_pos_ = div_fade_pos_ = 0;
    cur_loop_len_ = old_loop_len_ = 0;
}

void Stutter::SetBuffers(float* left, float* right, size_t size) {
    buf_l_ = left;
    buf_r_ = right;
    size_  = size;
    for (size_t i = 0; i < size; ++i) {
        left[i]  = 0.0f;
        right[i] = 0.0f;
    }
    write_pos_ = bar_start_pos_ = 0;
}

void Stutter::SetAmount(float amount) {
    amount_ = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
}

void Stutter::SetBeatSamples(uint32_t samples) {
    // Two full bars (8 beats) must fit in the ring: slices anchor up to one
    // bar back from the current position
    const uint32_t max_beat = size_ ? static_cast<uint32_t>(size_ / 8) : samples;
    if (samples > max_beat) samples = max_beat;
    if (samples < kMinLoopSamples) samples = kMinLoopSamples;
    beat_samples_ = samples;
    if (grid_pos_ >= BarLen()) grid_pos_ %= BarLen();
}

void Stutter::SyncBeat() {
    // Snap to the nearest beat boundary; corrects free-run drift while
    // leaving the bar phase (set by SyncDownbeat) intact
    const uint32_t beat = beat_samples_;
    const uint32_t k = (grid_pos_ + beat / 2) / beat;
    grid_pos_ = (k % 4) * beat;
}

void Stutter::SyncDownbeat() {
    grid_pos_ = 0;
}

void Stutter::AdvanceGrid() {
    if (++grid_pos_ >= BarLen()) grid_pos_ = 0;
}

uint32_t Stutter::LoopLengthSamples() const {
    int k = static_cast<int>(amount_ * static_cast<float>(kNumDivisions));
    if (k >= kNumDivisions) k = kNumDivisions - 1;
    uint32_t len = BarLen() >> k;
    if (len < kMinLoopSamples) len = kMinLoopSamples;
    return len;
}

float Stutter::ReadRing(const float* buf, int64_t offset_from_bar_start) const {
    int64_t idx = (static_cast<int64_t>(bar_start_pos_) + offset_from_bar_start)
                  % static_cast<int64_t>(size_);
    if (idx < 0) idx += static_cast<int64_t>(size_);
    return buf[idx];
}

// Phase-locked repeat of the last slice completed before the engage point:
// output grid slot g plays frozen slot anchor + (g mod L), with mini fades
// at the slice edges
float Stutter::SliceRead(const float* buf, uint32_t loop_len) const {
    const uint32_t phase = grid_pos_ % loop_len;
    const int64_t anchor = (static_cast<int64_t>(engage_grid_ / loop_len) - 1)
                           * static_cast<int64_t>(loop_len);
    return ReadRing(buf, anchor + static_cast<int64_t>(phase))
           * EdgeEnv(phase, loop_len);
}

void Stutter::Record(float left, float right) {
    if (!buf_l_ || !buf_r_ || size_ == 0) return;

    if (active_) {
        // Resuming after a freeze: re-map the grid onto the ring as if
        // recording had never stopped, so grid slots keep pointing at
        // grid-coherent (previous bar) material until fresh audio lands
        active_ = false;
        releasing_ = false;
        bar_start_pos_ = (write_pos_ + static_cast<uint32_t>(size_) - grid_pos_)
                         % static_cast<uint32_t>(size_);
    }

    if (grid_pos_ == 0) bar_start_pos_ = write_pos_;

    buf_l_[write_pos_] = left;
    buf_r_[write_pos_] = right;
    write_pos_ = (write_pos_ + 1) % size_;
    AdvanceGrid();
}

void Stutter::Process(float& left, float& right) {
    if (!buf_l_ || !buf_r_ || size_ == 0) return;

    const bool want = amount_ > kActiveThreshold;

    if (!active_) {
        if (!want) {
            Record(left, right);
            return;
        }
        // Engage immediately: playback is phase-locked to the grid anyway
        active_           = true;
        releasing_        = false;
        engage_grid_      = grid_pos_;
        engage_fade_pos_  = 0;
        release_fade_pos_ = 0;
        cur_loop_len_     = LoopLengthSamples();
        old_loop_len_     = 0;
        div_fade_pos_     = kFadeSamples;
    }

    // Crossfade division changes so sweeping the knob never clicks
    const uint32_t L = LoopLengthSamples();
    if (want && L != cur_loop_len_) {
        old_loop_len_ = cur_loop_len_;
        cur_loop_len_ = L;
        div_fade_pos_ = 0;
    }

    float wetL = SliceRead(buf_l_, cur_loop_len_);
    float wetR = SliceRead(buf_r_, cur_loop_len_);
    if (div_fade_pos_ < kFadeSamples && old_loop_len_ != 0) {
        const float t = static_cast<float>(div_fade_pos_++) / kFadeSamples;
        wetL = Mix(SliceRead(buf_l_, old_loop_len_), wetL, t);
        wetR = Mix(SliceRead(buf_r_, old_loop_len_), wetR, t);
    }

    if (!want && !releasing_) {
        releasing_        = true;
        release_fade_pos_ = 0;
    }

    if (releasing_) {
        // Wet fades back into the live input, then recording resumes
        const float t = static_cast<float>(release_fade_pos_++) / kFadeSamples;
        left  = Mix(wetL, left, t);
        right = Mix(wetR, right, t);
        if (release_fade_pos_ >= kFadeSamples) active_ = false;
    } else if (engage_fade_pos_ < kFadeSamples) {
        const float t = static_cast<float>(engage_fade_pos_++) / kFadeSamples;
        left  = Mix(left, wetL, t);
        right = Mix(right, wetR, t);
    } else {
        left  = wetL;
        right = wetR;
    }

    AdvanceGrid();
}

void Stutter::ProcessReverse(float& left, float& right) {
    if (!buf_l_ || !buf_r_ || size_ == 0) return;

    const bool want   = amount_ > kActiveThreshold;
    const bool onBeat = (grid_pos_ % beat_samples_) == 0;

    if (!active_) {
        // Engage only on a beat boundary so the reverse always enters on
        // the grid
        if (want && onBeat) {
            active_           = true;
            releasing_        = false;
            engage_fade_pos_  = 0;
            release_fade_pos_ = 0;
        } else {
            Record(left, right);
            return;
        }
    }

    // Disengage waits for the next beat boundary: close the knob anywhere
    // in the last beat and the dry signal slams back exactly on the grid
    if (!want && !releasing_ && onBeat) {
        releasing_        = true;
        release_fade_pos_ = 0;
    }

    // Mirror the bar in place: grid slot g plays frozen slot (bar-1-g), so
    // bar boundaries always land on the downbeat
    const uint32_t bar = BarLen();
    const int64_t offset = static_cast<int64_t>(bar) - 1
                           - static_cast<int64_t>(grid_pos_);
    const float env = EdgeEnv(grid_pos_, bar);
    float wetL = ReadRing(buf_l_, offset) * env;
    float wetR = ReadRing(buf_r_, offset) * env;

    if (releasing_) {
        const float t = static_cast<float>(release_fade_pos_++) / kFadeSamples;
        left  = Mix(wetL, left, t);
        right = Mix(wetR, right, t);
        if (release_fade_pos_ >= kFadeSamples) active_ = false;
    } else if (engage_fade_pos_ < kFadeSamples) {
        const float t = static_cast<float>(engage_fade_pos_++) / kFadeSamples;
        left  = Mix(left, wetL, t);
        right = Mix(right, wetR, t);
    } else {
        left  = wetL;
        right = wetR;
    }

    AdvanceGrid();
}
