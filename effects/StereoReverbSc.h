#pragma once
#ifndef SWF_STEREOREVERBSC_H
#define SWF_STEREOREVERBSC_H

#include "daisysp.h"
#include "daisysp-lgpl.h"


using namespace daisysp;

namespace sudwalfulkaan {
    class StereoReverbSc {
    public:
        void Init(float sample_rate, float* buffer, size_t size);
        void SetFeedback(float feedback);
        void SetLowpassFreq(float freq);
        int Process(const float& inL, const float& inR, float* wetL, float* wetR);

    private:
        ReverbSc reverb_;
    };
}

#endif // SWF_STEREOREVERBSC_H
