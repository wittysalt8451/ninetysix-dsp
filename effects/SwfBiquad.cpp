#include "SwfBiquad.h"
#include "daisysp.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace sudwalfulkaan;

void sudwalfulkaan::SwfBiquad::Init(float sample_rate, SwfBiquad::Type type) {
    sample_rate_ = sample_rate;
    type_ = type;
    z1_ = z2_ = 0.0f;
    CalcCoefficients();
}

void sudwalfulkaan::SwfBiquad::SetType(SwfBiquad::Type type) {
    type_ = type;
    CalcCoefficients();
}

void sudwalfulkaan::SwfBiquad::SetFreq(float freq) {
    freq_ = daisysp::fclamp(freq, 10.0f, sample_rate_ * 0.45f); // Nyquist protection
    CalcCoefficients();
}

void sudwalfulkaan::SwfBiquad::SetQ(float q) {
    q_ = daisysp::fclamp(q, 0.01f, 10.0f); // avoid div by 0
    CalcCoefficients();
}

void sudwalfulkaan::SwfBiquad::SetGainDb(float gain_db) {
    gain_db_ = daisysp::fclamp(gain_db, -24.0f, 24.0f);
    CalcCoefficients();
}

float sudwalfulkaan::SwfBiquad::Process(float in) {
    float out = b0_ * in + z1_;
    z1_ = b1_ * in - a1_ * out + z2_;
    z2_ = b2_ * in - a2_ * out;
    return out;
}

void sudwalfulkaan::SwfBiquad::CalcCoefficients() {
    float A = std::pow(10.0f, gain_db_ / 40.0f);
    float omega = 2.0f * M_PI * freq_ / sample_rate_;
    float sn = std::sin(omega);
    float cs = std::cos(omega);
    float alpha = sn / (2.0f * q_);
    float beta = std::sqrt(A + A);

    switch (type_) {
        case LOWPASS:
            b0_ = (1 - cs) / 2.0f;
            b1_ = 1 - cs;
            b2_ = (1 - cs) / 2.0f;
            a0_ = 1 + alpha;
            a1_ = -2 * cs;
            a2_ = 1 - alpha;
            break;

        case HIGHPASS:
            b0_ = (1 + cs) / 2.0f;
            b1_ = -(1 + cs);
            b2_ = (1 + cs) / 2.0f;
            a0_ = 1 + alpha;
            a1_ = -2 * cs;
            a2_ = 1 - alpha;
            break;

        case BANDPASS:
            b0_ = alpha;
            b1_ = 0.0f;
            b2_ = -alpha;
            a0_ = 1 + alpha;
            a1_ = -2 * cs;
            a2_ = 1 - alpha;
            break;

        case NOTCH:
            b0_ = 1;
            b1_ = -2 * cs;
            b2_ = 1;
            a0_ = 1 + alpha;
            a1_ = -2 * cs;
            a2_ = 1 - alpha;
            break;

        case PEAK:
            b0_ = 1 + alpha * A;
            b1_ = -2 * cs;
            b2_ = 1 - alpha * A;
            a0_ = 1 + alpha / A;
            a1_ = -2 * cs;
            a2_ = 1 - alpha / A;
            break;

        case LOWSHELF:
            b0_ = A * ((A + 1) - (A - 1) * cs + beta * sn);
            b1_ = 2 * A * ((A - 1) - (A + 1) * cs);
            b2_ = A * ((A + 1) - (A - 1) * cs - beta * sn);
            a0_ = (A + 1) + (A - 1) * cs + beta * sn;
            a1_ = -2 * ((A - 1) + (A + 1) * cs);
            a2_ = (A + 1) + (A - 1) * cs - beta * sn;
            break;

        case HIGHSHELF:
            b0_ = A * ((A + 1) + (A - 1) * cs + beta * sn);
            b1_ = -2 * A * ((A - 1) + (A + 1) * cs);
            b2_ = A * ((A + 1) + (A - 1) * cs - beta * sn);
            a0_ = (A + 1) - (A - 1) * cs + beta * sn;
            a1_ = 2 * ((A - 1) - (A + 1) * cs);
            a2_ = (A + 1) - (A - 1) * cs - beta * sn;
            break;
    }

    // Protect against division by zero
    if (std::abs(a0_) < 1e-8f) {
        b0_ = 0.0f;
        b1_ = 0.0f;
        b2_ = 0.0f;
        a1_ = 0.0f;
        a2_ = 0.0f;
        return;
    }

    // Normalize
    b0_ /= a0_;
    b1_ /= a0_;
    b2_ /= a0_;
    a1_ /= a0_;
    a2_ /= a0_;
    a0_ = 1.0f;
}
