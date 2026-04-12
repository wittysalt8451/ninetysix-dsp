#pragma once
#ifndef SWF_BIQUAD_FILTER_H
#define SWF_BIQUAD_FILTER_H

#include <stdint.h>
#ifdef __cplusplus
#include <cmath>

namespace sudwalfulkaan {

    class SwfBiquad {
        public:
            enum Type {
                LOWPASS,
                HIGHPASS,
                BANDPASS,
                NOTCH,
                PEAK,
                LOWSHELF,
                HIGHSHELF
            };

            void Init(float sample_rate, Type type = LOWPASS);

            void SetType(Type type);
            void SetFreq(float freq);
            void SetQ(float q);
            void SetGainDb(float gain_db);

            float Process(float in);

        private:
            void CalcCoefficients();

            Type type_;
            float sample_rate_;
            float freq_ = 1000.0f;
            float q_ = 0.707f;
            float gain_db_ = 0.0f;

            float a0_, a1_, a2_;
            float b0_, b1_, b2_;

            float z1_ = 0.0f, z2_ = 0.0f;
    };

} // namespace sudwalfulkaan


#endif
#endif
