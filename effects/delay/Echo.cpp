#include "delay/Echo.h"

#include <math.h>

using namespace ninetysix;

namespace {
    // Repeats last longer as the amount opens: at full amount each repeat
    // is ~2.5 dB down, about 16 audible repeats
    constexpr float kFeedbackMin   = 0.3f;
    constexpr float kFeedbackMax   = 0.75f;
    // Loop filters: every pass loses a little bass and top
    constexpr float kLowCutHz      = 120.0f;
    constexpr float kHighCutHz     = 5000.0f;
    constexpr float kSmoothSeconds = 0.01f;
    constexpr float kXfadeSeconds  = 0.02f;
    // Silence threshold for the tail (-80 dB) and a safety bound on the
    // loop (sustained tones on the beat can stack up to 4x)
    constexpr float kTailFloor     = 0.0001f;
    constexpr float kLoopLimit     = 4.0f;
    constexpr float kTwoPi         = 6.28318530718f;

    inline float Mix(float a, float b, float t) { return a + (b - a) * t; }
    inline float OnePoleCoef(float hz, float sample_rate) {
        return 1.0f - expf(-kTwoPi * hz / sample_rate);
    }
}

void Echo::Init(float sample_rate, float* left, float* right, size_t size) {
    buf_l_ = left;
    buf_r_ = right;
    size_  = size;
    for (size_t i = 0; i < size; ++i) {
        left[i]  = 0.0f;
        right[i] = 0.0f;
    }
    write_pos_ = 0;

    smooth_    = 1.0f - expf(-1.0f / (kSmoothSeconds * sample_rate));
    lp_coef_   = OnePoleCoef(kHighCutHz, sample_rate);
    hp_coef_   = OnePoleCoef(kLowCutHz, sample_rate);
    xfade_len_ = static_cast<uint32_t>(kXfadeSeconds * sample_rate);
    if (xfade_len_ < 1) xfade_len_ = 1;
    xfade_pos_ = xfade_len_;

    lp_l_ = hp_l_ = lp_r_ = hp_r_ = 0.0f;
    send_ = wet_ = 0.0f;
    input_on_ = false;
    pass_peak_ = last_pass_peak_ = 0.0f;
    pass_pos_ = 0;

    SetAmount(0.0f);
    feedback_ = kFeedbackMin;
    SetDelaySamples(static_cast<uint32_t>(sample_rate * 0.5f)); // 120 BPM beat
    delay_ = old_delay_ = pending_delay_;
}

void Echo::SetDelaySamples(uint32_t samples) {
    if (size_ < 2) return;
    if (samples < 1) samples = 1;
    if (samples > size_ - 1) samples = static_cast<uint32_t>(size_ - 1);
    pending_delay_ = samples; // picked up by Process, one crossfade at a time
}

void Echo::SetAmount(float amount) {
    amount_ = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
}

bool Echo::IsRinging() const {
    const float peak = pass_peak_ > last_pass_peak_ ? pass_peak_ : last_pass_peak_;
    return input_on_ || send_ > kTailFloor || peak > kTailFloor;
}

float Echo::Read(const float* buf, uint32_t delay) const {
    size_t idx = write_pos_ + size_ - delay;
    if (idx >= size_) idx -= size_;
    return buf[idx];
}

// One pass of the feedback loop: high cut, then low cut (`hp` tracks the
// bass that gets taken out)
float Echo::Loop(float in, float& lp, float& hp) const {
    lp += lp_coef_ * (in - lp);
    hp += hp_coef_ * (lp - hp);
    return lp - hp;
}

void Echo::Process(float& left, float& right) {
    if (!buf_l_ || !buf_r_ || size_ < 2) return;

    // Smooth the control-rate parameters so knob moves never zipper
    const float feedback = kFeedbackMin + (kFeedbackMax - kFeedbackMin) * amount_;
    send_     += smooth_ * ((input_on_ ? 1.0f : 0.0f) - send_);
    wet_      += smooth_ * (amount_ - wet_);
    feedback_ += smooth_ * (feedback - feedback_);

    // A new delay fades in from the old read head
    if (xfade_pos_ >= xfade_len_ && pending_delay_ != delay_) {
        old_delay_ = delay_;
        delay_     = pending_delay_;
        xfade_pos_ = 0;
    }
    float echo_l = Read(buf_l_, delay_);
    float echo_r = Read(buf_r_, delay_);
    if (xfade_pos_ < xfade_len_) {
        const float t = static_cast<float>(xfade_pos_++) / static_cast<float>(xfade_len_);
        echo_l = Mix(Read(buf_l_, old_delay_), echo_l, t);
        echo_r = Mix(Read(buf_r_, old_delay_), echo_r, t);
    }

    float in_l = left * send_ + feedback_ * Loop(echo_l, lp_l_, hp_l_);
    float in_r = right * send_ + feedback_ * Loop(echo_r, lp_r_, hp_r_);
    in_l = in_l > kLoopLimit ? kLoopLimit : (in_l < -kLoopLimit ? -kLoopLimit : in_l);
    in_r = in_r > kLoopLimit ? kLoopLimit : (in_r < -kLoopLimit ? -kLoopLimit : in_r);
    buf_l_[write_pos_] = in_l;
    buf_r_[write_pos_] = in_r;
    if (++write_pos_ >= size_) write_pos_ = 0;

    // Track the loudest sample per pass through the delay: once a whole
    // pass stays below the floor, the tail is over
    const float peak = fabsf(in_l) > fabsf(in_r) ? fabsf(in_l) : fabsf(in_r);
    if (peak > pass_peak_) pass_peak_ = peak;
    if (++pass_pos_ >= delay_) {
        last_pass_peak_ = pass_peak_;
        pass_peak_ = 0.0f;
        pass_pos_  = 0;
    }

    left  += wet_ * echo_l;
    right += wet_ * echo_r;
}
