#pragma once
#ifndef SWF_FDN4REVERB_H
#define SWF_FDN4REVERB_H

#include <cstddef>

namespace ninetysix {
    // 4-line feedback delay network reverb (Householder feedback matrix).
    // Replaces the DaisySP ReverbSc wrapper: no external dependencies, all
    // delay memory lives in a caller-provided buffer (SDRAM on Daisy).
    //
    // Mono-summed input, decorrelated stereo output. The Householder matrix
    // is orthogonal, so with per-line gains < 1 the loop cannot run away,
    // even at Decay = 1.
    class FDN4Reverb {
    public:
        static constexpr size_t kNumLines = 4;
        // Extra samples per line for delay modulation + interpolation
        static constexpr size_t kModHeadroom = 16;
        // Line lengths at 48 kHz: mutually prime, 37.5..60.8 ms
        static constexpr size_t kNominalLengths[kNumLines] = {1801, 2111, 2503, 2917};
        // Floats needed for full-length lines at <= 48 kHz (~36.7 KB)
        static constexpr size_t kBufferSize =
            1801 + 2111 + 2503 + 2917 + kNumLines * kModHeadroom;

        void Init(float sample_rate, float* buffer, size_t size);
        void SetDecay(float decay);        // 0..1 -> T60 ~0.2 s .. ~4 s
        void SetDamping(float damping);    // 0 = dark tail, 1 = bright tail
        void SetModulation(float amount);  // 0 = static, 1 = max delay modulation
        void SetMix(float mix);            // 0..1 dry/wet, equal-power crossfade
        // In-place safe: outL/outR may alias the inputs
        void Process(float inL, float inR, float* outL, float* outR);

    private:
        float ReadLine(size_t line, float delay) const;

        float* line_[kNumLines];
        size_t cap_[kNumLines];
        size_t len_[kNumLines];
        size_t write_[kNumLines];
        float gain_[kNumLines];       // per-line feedback gain from decay time
        float lp_[kNumLines];         // damping one-pole state
        float lfo_phase_[kNumLines];
        float lfo_inc_[kNumLines];
        float sample_rate_;
        float decay_;
        float damp_coef_;
        float mod_depth_target_;
        float mod_depth_;
        float dry_gain_, wet_gain_;
        float dc_x1_, dc_y1_;         // input DC blocker state
        bool bypass_;                 // set when no usable buffer was provided
    };
}

#endif // SWF_FDN4REVERB_H
