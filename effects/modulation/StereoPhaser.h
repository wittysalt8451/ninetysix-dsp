#pragma once

#include <cstddef>
#include "utils/Lfo.h"

namespace ninetysix
{
    class StereoPhaser
    {
      public:
        void Init(float sample_rate);
        void SetDelays(float* buffer, size_t size);

        void SetFreq(float freq);
        void SetFeedback(float feedback);
        void SetDepth(float depth);
        void SetMix(float mix);

        float ProcessLeft(float in);
        float ProcessRight(float in);

      private:
        float sample_rate_;

        float freq_     = 0.5f;
        float feedback_ = 0.3f;
        float depth_    = 0.7f;
        float mix_      = 0.5f;

        float prevL_ = 0.f, prevR_ = 0.f;

        Lfo lfoL_, lfoR_;

        // Manual delay line implementation for SDRAM
        float* delay_buffer_ = nullptr;
        size_t delay_size_ = 0;
        size_t write_pos_L_ = 0;
        size_t write_pos_R_ = 0;
    };
}
