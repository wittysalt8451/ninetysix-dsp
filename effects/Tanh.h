#pragma once
#ifndef SWF_TANH_SATURATION_H
#define SWF_TANH_SATURATION_H

#include <stdint.h>
#ifdef __cplusplus
#include <cmath>

namespace sudwalfulkaan {
    /** Tanh Saturation Effect */
    class TanhSaturation
    {
    public:
        TanhSaturation() {}
        ~TanhSaturation() {}

        /** Initializes the tanh saturation module with default settings. */
        void Init();

        /** Applies tanh saturation to input signal. */
        float Process(float in);

        /**
            \param gain : sets input gain before applying tanh saturation
        */
        inline void SetGain(float gain) { gain_ = gain; }
        /**
            \param asymmetry : controls asymmetry (-1.0 to 1.0, 0.0 = symmetric)
        */
        inline void SetAsymmetry(float asymmetry) { asymmetry_ = asymmetry; }

    private:
        float gain_;       // Input gain
        float asymmetry_;  // Asymmetry factor (-1 to 1)
    };
}
#endif
#endif
