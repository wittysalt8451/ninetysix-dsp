#pragma once
#ifndef SWF_STEREOCHORUS_H
#define SWF_STEREOCHORUS_H

#include "daisysp.h"

using namespace daisysp;

namespace sudwalfulkaan {
    class StereoChorus {
    public:
        void Init(float sample_rate);
        void SetRate(float rate);
        void SetDepth(float depth);
        void SetIntensity(float intensity);
        void SetMix(float mix);
        void SetFeedback(float feedback);
        float ProcessLeft(float in);
        float ProcessRight(float in);

    private:
        static constexpr size_t MAX_DELAY = 480; // Max delay samples (~10ms @ 48kHz)

        DelayLine<float, MAX_DELAY> delayL, delayR;
        Oscillator lfoL, lfoR;
        float sample_rate_;
        float rate_, depth_, mix_, feedback_, intensity_;
    };
}
#endif // SWF_STEREOCHORUS_H
