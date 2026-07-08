#include "effects/reverb/FDN4Reverb.h"

#include <cmath>

using namespace ninetysix;

namespace {
    constexpr float kTwoPi = 6.28318530717958647692f;
    // Mutually detuned LFO rates (Hz): fast enough to break up comb
    // resonances, slow enough that the pitch deviation stays inaudible
    // (~1.5 cents peak at full depth).
    constexpr float kLfoRates[FDN4Reverb::kNumLines] = {0.61f, 0.83f, 0.97f, 1.19f};
    constexpr float kLfoPhases[FDN4Reverb::kNumLines] = {0.0f, 0.25f, 0.5f, 0.75f};
    constexpr float kMaxModDepthSamples = 9.0f;
    constexpr size_t kMinLineLength = 64;

    float Clamp(float x, float lo, float hi) {
        return x < lo ? lo : (x > hi ? hi : x);
    }

    // sin(2*pi*phase) for phase in [0,1), parabolic approximation
    float ParabolicSine(float phase) {
        float x = 2.0f * phase - 1.0f;
        return -4.0f * x * (1.0f - fabsf(x));
    }
}

void FDN4Reverb::Init(float sample_rate, float* buffer, size_t size) {
    sample_rate_ = sample_rate > 0.0f ? sample_rate : 48000.0f;
    bypass_ = (buffer == nullptr) ||
              (size < kNumLines * (kMinLineLength + kModHeadroom));
    if (bypass_) {
        for (size_t i = 0; i < kNumLines; i++) {
            line_[i] = nullptr;
            len_[i] = kMinLineLength;
        }
    } else {
        // Scale the 48 kHz reference lengths to the actual sample rate,
        // then shrink proportionally if the buffer cannot hold them.
        const float rate_scale = sample_rate_ / 48000.0f;
        size_t needed = 0;
        for (size_t i = 0; i < kNumLines; i++) {
            len_[i] = static_cast<size_t>(
                static_cast<float>(kNominalLengths[i]) * rate_scale + 0.5f);
            if (len_[i] < kMinLineLength) len_[i] = kMinLineLength;
            needed += len_[i] + kModHeadroom;
        }
        if (needed > size) {
            const float shrink =
                static_cast<float>(size - kNumLines * kModHeadroom) /
                static_cast<float>(needed - kNumLines * kModHeadroom);
            needed = 0;
            for (size_t i = 0; i < kNumLines; i++) {
                len_[i] = static_cast<size_t>(static_cast<float>(len_[i]) * shrink);
                if (len_[i] < kMinLineLength) len_[i] = kMinLineLength;
                needed += len_[i] + kModHeadroom;
            }
            // Rounding can leave us a few samples over; trim the longest lines
            for (size_t i = kNumLines; needed > size && i-- > 0;) {
                const size_t excess = needed - size;
                const size_t cut = len_[i] - kMinLineLength < excess
                                       ? len_[i] - kMinLineLength
                                       : excess;
                len_[i] -= cut;
                needed -= cut;
            }
            bypass_ = needed > size;
        }
        float* p = buffer;
        for (size_t i = 0; i < kNumLines; i++) {
            cap_[i] = len_[i] + kModHeadroom;
            line_[i] = p;
            p += cap_[i];
        }
        for (size_t i = 0; i < needed; i++) {
            buffer[i] = 0.0f;
        }
    }

    for (size_t i = 0; i < kNumLines; i++) {
        write_[i] = 0;
        lp_[i] = 0.0f;
        lfo_phase_[i] = kLfoPhases[i];
        lfo_inc_[i] = kLfoRates[i] / sample_rate_;
    }
    dc_x1_ = dc_y1_ = 0.0f;
    mod_depth_ = 0.0f;

    SetDecay(0.5f);
    SetDamping(0.7f);
    SetModulation(0.3f);
    SetMix(0.5f);
}

void FDN4Reverb::SetDecay(float decay) {
    decay_ = Clamp(decay, 0.0f, 1.0f);
    // 0 -> ~0.2 s, 1 -> ~4 s T60. The per-line gains this produces stay
    // below 0.94 even at Decay = 1, so the loop is unconditionally stable.
    const float t60 = 0.2f * powf(20.0f, decay_);
    for (size_t i = 0; i < kNumLines; i++) {
        gain_[i] = powf(10.0f, -3.0f * static_cast<float>(len_[i]) /
                                   (t60 * sample_rate_));
    }
}

void FDN4Reverb::SetDamping(float damping) {
    // 0 -> 1.2 kHz (dark), 1 -> 14.4 kHz (bright), on the tail only
    const float fc = 1200.0f * powf(12.0f, Clamp(damping, 0.0f, 1.0f));
    damp_coef_ = 1.0f - expf(-kTwoPi * fc / sample_rate_);
}

void FDN4Reverb::SetModulation(float amount) {
    mod_depth_target_ = Clamp(amount, 0.0f, 1.0f) * kMaxModDepthSamples;
}

void FDN4Reverb::SetMix(float mix) {
    const float m = Clamp(mix, 0.0f, 1.0f) * (0.25f * kTwoPi);
    dry_gain_ = cosf(m);
    wet_gain_ = sinf(m);
}

float FDN4Reverb::ReadLine(size_t line, float delay) const {
    float rp = static_cast<float>(write_[line]) - delay;
    if (rp < 0.0f) rp += static_cast<float>(cap_[line]);
    const size_t i0 = static_cast<size_t>(rp);
    const float frac = rp - static_cast<float>(i0);
    size_t i1 = i0 + 1;
    if (i1 >= cap_[line]) i1 = 0;
    const float* b = line_[line];
    return b[i0] + frac * (b[i1] - b[i0]);
}

void FDN4Reverb::Process(float inL, float inR, float* outL, float* outR) {
    const float dryL = inL;
    const float dryR = inR;
    if (bypass_) {
        *outL = dryL;
        *outR = dryR;
        return;
    }

    // Mono-summed input, DC-blocked so offsets cannot build up in the loop
    const float mono = 0.5f * (inL + inR);
    const float dc = mono - dc_x1_ + 0.995f * dc_y1_;
    dc_x1_ = mono;
    dc_y1_ = dc;
    const float inject = 0.5f * dc;

    // Smooth depth changes so a knob turn cannot step the read positions
    mod_depth_ += 0.001f * (mod_depth_target_ - mod_depth_);

    float d[kNumLines];
    for (size_t i = 0; i < kNumLines; i++) {
        lfo_phase_[i] += lfo_inc_[i];
        if (lfo_phase_[i] >= 1.0f) lfo_phase_[i] -= 1.0f;
        const float delay = static_cast<float>(len_[i]) +
                            mod_depth_ * ParabolicSine(lfo_phase_[i]);
        d[i] = ReadLine(i, delay);
    }

    // Householder feedback matrix: y_i = d_i - 0.5 * sum(d). Orthogonal,
    // so energy is preserved and stability reduces to gain_[i] < 1.
    const float s = 0.5f * (d[0] + d[1] + d[2] + d[3]);
    for (size_t i = 0; i < kNumLines; i++) {
        lp_[i] += damp_coef_ * ((d[i] - s) - lp_[i]);
        if (fabsf(lp_[i]) < 1e-20f) lp_[i] = 0.0f; // denormal flush
        const float v = Clamp(lp_[i] * gain_[i] + inject, -3.0f, 3.0f);
        line_[i][write_[i]] = v;
        if (++write_[i] >= cap_[i]) write_[i] = 0;
    }

    // Disjoint tap pairs per channel: decorrelated stereo, no cancellation
    // in the mono sum
    const float wetL = 0.6f * (d[0] + d[2]);
    const float wetR = 0.6f * (d[1] + d[3]);
    *outL = Clamp(dryL * dry_gain_ + wetL * wet_gain_, -1.0f, 1.0f);
    *outR = Clamp(dryR * dry_gain_ + wetR * wet_gain_, -1.0f, 1.0f);
}
