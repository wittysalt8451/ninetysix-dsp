#include "filters/Biquad.h"
#include "daisysp.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace ninetysix;

void ninetysix::Biquad::Init(float sample_rate, Biquad::Type type) {
    sample_rate_ = sample_rate;
    type_ = type;
    z1_ = z2_ = 0.0f;
    CalcCoefficients();
}

void ninetysix::Biquad::SetType(Biquad::Type type) {
    type_ = type;
    CalcCoefficients();
}

void ninetysix::Biquad::SetFreq(float freq) {
    freq_ = daisysp::fclamp(freq, 10.0f, sample_rate_ * 0.45f); // Nyquist protection
    CalcCoefficients();
}

void ninetysix::Biquad::SetQ(float q) {
    q_ = daisysp::fclamp(q, 0.01f, 10.0f); // avoid div by 0
    CalcCoefficients();
}

void ninetysix::Biquad::SetGainDb(float gain_db) {
    gain_db_ = daisysp::fclamp(gain_db, -24.0f, 24.0f);
    CalcCoefficients();
}

float ninetysix::Biquad::Process(float in) {
    float out = b0_ * in + z1_;
    z1_ = b1_ * in - a1_ * out + z2_;
    z2_ = b2_ * in - a2_ * out;
    return out;
}

void ninetysix::Biquad::CalcCoefficients() {
    float A = std::pow(10.0f, gain_db_ / 40.0f);
    float omega = 2.0f * M_PI * freq_ / sample_rate_;
    float sn = std::sin(omega);
    float cs = std::cos(omega);
    float alpha = sn / (2.0f * q_);
    float beta = std::sqrt(A + A);

    float a0, a1, a2, b0, b1, b2;

    switch (type_) {
        case LOWPASS:
            b0 = (1 - cs) / 2.0f;
            b1 = 1 - cs;
            b2 = (1 - cs) / 2.0f;
            a0 = 1 + alpha;
            a1 = -2 * cs;
            a2 = 1 - alpha;
            break;

        case HIGHPASS:
            b0 = (1 + cs) / 2.0f;
            b1 = -(1 + cs);
            b2 = (1 + cs) / 2.0f;
            a0 = 1 + alpha;
            a1 = -2 * cs;
            a2 = 1 - alpha;
            break;

        case BANDPASS:
            b0 = alpha;
            b1 = 0.0f;
            b2 = -alpha;
            a0 = 1 + alpha;
            a1 = -2 * cs;
            a2 = 1 - alpha;
            break;

        case NOTCH:
            b0 = 1;
            b1 = -2 * cs;
            b2 = 1;
            a0 = 1 + alpha;
            a1 = -2 * cs;
            a2 = 1 - alpha;
            break;

        case PEAK:
            b0 = 1 + alpha * A;
            b1 = -2 * cs;
            b2 = 1 - alpha * A;
            a0 = 1 + alpha / A;
            a1 = -2 * cs;
            a2 = 1 - alpha / A;
            break;

        case LOWSHELF:
            b0 = A * ((A + 1) - (A - 1) * cs + beta * sn);
            b1 = 2 * A * ((A - 1) - (A + 1) * cs);
            b2 = A * ((A + 1) - (A - 1) * cs - beta * sn);
            a0 = (A + 1) + (A - 1) * cs + beta * sn;
            a1 = -2 * ((A - 1) + (A + 1) * cs);
            a2 = (A + 1) + (A - 1) * cs - beta * sn;
            break;

        case HIGHSHELF:
        default:
            b0 = A * ((A + 1) + (A - 1) * cs + beta * sn);
            b1 = -2 * A * ((A - 1) + (A + 1) * cs);
            b2 = A * ((A + 1) + (A - 1) * cs - beta * sn);
            a0 = (A + 1) - (A - 1) * cs + beta * sn;
            a1 = 2 * ((A - 1) - (A + 1) * cs);
            a2 = (A + 1) - (A - 1) * cs - beta * sn;
            break;
    }

    // Protect against division by zero
    if (std::abs(a0) < 1e-8f) {
        b0_ = 0.0f;
        b1_ = 0.0f;
        b2_ = 0.0f;
        a1_ = 0.0f;
        a2_ = 0.0f;
        return;
    }

    // Normalize into locals first: the audio ISR calls Process() and may
    // interrupt this function, so the member coefficients must never hold a
    // partially computed (un-normalized, potentially unstable) set.
    float inv_a0 = 1.0f / a0;
    b0_ = b0 * inv_a0;
    b1_ = b1 * inv_a0;
    b2_ = b2 * inv_a0;
    a1_ = a1 * inv_a0;
    a2_ = a2 * inv_a0;
    a0_ = 1.0f;
}
