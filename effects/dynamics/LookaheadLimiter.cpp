#include "dynamics/LookaheadLimiter.h"
#include <cmath>

using namespace ninetysix;

void LookaheadLimiter::Init(float sample_rate, float lookahead_ms, float release_ms) {
    length_ = static_cast<int>(lookahead_ms * 0.001f * sample_rate + 0.5f);
    if (length_ < 1) length_ = 1;
    if (length_ > kMaxLookahead) length_ = kMaxLookahead;
    inv_length_ = 1.0f / length_;
    release_ = 1.0f - std::exp(-1000.0f / (release_ms * sample_rate));

    for (int i = 0; i < kMaxLookahead; ++i) {
        delay_l_[i] = 0.0f;
        delay_r_[i] = 0.0f;
        box_[i] = 1.0f;
    }
    box_sum_ = static_cast<float>(length_);
    released_ = 1.0f;
    pos_ = 0;
    min_head_ = 0;
    min_count_ = 0;
    now_ = 0;
}

void LookaheadLimiter::Process(float& left, float& right) {
    const float l = left * drive_;
    const float r = right * drive_;

    // The gain this sample needs to stay under the ceiling
    const float al = l < 0.0f ? -l : l;
    const float ar = r < 0.0f ? -r : r;
    const float peak = al > ar ? al : ar;
    const float wanted = peak > ceiling_ ? ceiling_ / peak : 1.0f;

    // Hold the lowest of those over the window: it covers every sample
    // still waiting in the delay line. Expire first, so the queue never
    // holds more than the window
    while (min_count_ > 0 && now_ - min_time_[min_head_] >= static_cast<uint32_t>(length_)) {
        min_head_ = (min_head_ + 1) % kMaxLookahead;
        --min_count_;
    }
    while (min_count_ > 0
           && min_gain_[(min_head_ + min_count_ - 1) % kMaxLookahead] >= wanted) {
        --min_count_;
    }
    const int back = (min_head_ + min_count_) % kMaxLookahead;
    min_gain_[back] = wanted;
    min_time_[back] = now_;
    ++min_count_;
    ++now_;
    const float held = min_gain_[min_head_];

    // Down at once, back up with the release
    released_ = held < released_ ? held : released_ + (held - released_) * release_;

    // Average over the window: the gain glides down over the lookahead and
    // still reaches the held value by the time its peak comes out
    box_sum_ += released_ - box_[pos_];
    box_[pos_] = released_;
    delay_l_[pos_] = l;
    delay_r_[pos_] = r;
    if (++pos_ == length_) {
        pos_ = 0;
        // Rebuild the running sum now and then, so rounding cannot drift
        box_sum_ = 0.0f;
        for (int i = 0; i < length_; ++i) box_sum_ += box_[i];
    }

    // pos_ now holds the sample written length_ - 1 samples ago
    const float gain = box_sum_ * inv_length_;
    left = delay_l_[pos_] * gain;
    right = delay_r_[pos_] * gain;
}
