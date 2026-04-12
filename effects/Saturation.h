#pragma once
#ifndef SATURATION_H
#define SATURATION_H

#include <cmath>
#include <algorithm>

namespace sudwalfulkaan {
    class Saturation {
    public:
        static const int SOFT_CLIPPING = 1;
        static const int TANH_SATURATION = 2;
        static const int ARCTAN_SATURATION = 3;
        static const int CUBIC_SATURATION = 4;
        static const int QUADRATIC_SATURATION = 5;
        static const int SINE_SATURATION = 6;
        static const int EXP_SATURATION = 7;
        static const int HARD_CLIP = 8;
        static const int RECTIFIER_SATURATION = 10;

        void Init(float gain, float drywet, int mode);
        void setDryWet(float drywet);
        void SetGain(float gain);
        int GetMode();
        void SetMode(float mode);
        float Process(float in);

    private:
        float gain_;
        int mode_;
        float drywet_;

        float ApplySaturation(float x);
    };
}

#endif // SATURATION_H