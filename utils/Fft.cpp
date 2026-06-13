#include "utils/Fft.h"

#include <cmath>
#include <utility>

namespace sudwalfulkaan {

namespace {
constexpr double kTwoPi = 6.283185307179586476925286766559;
}

bool Fft::IsPowerOfTwo(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

bool Fft::Init(size_t size, float* twiddleCos, float* twiddleSin) {
    if (!IsPowerOfTwo(size) || size < kMinSize
        || twiddleCos == nullptr || twiddleSin == nullptr) {
        return false;
    }

    const size_t half = size / 2;
    for (size_t k = 0; k < half; ++k) {
        const double angle = -kTwoPi * static_cast<double>(k)
                           / static_cast<double>(size);
        twiddleCos[k] = static_cast<float>(std::cos(angle));
        twiddleSin[k] = static_cast<float>(std::sin(angle));
    }

    size_ = size;
    twiddleCos_ = twiddleCos;
    twiddleSin_ = twiddleSin;
    return true;
}

void Fft::Forward(float* real, float* imag) const {
    Transform(real, imag, false);
}

void Fft::Inverse(float* real, float* imag) const {
    Transform(real, imag, true);

    const float scale = 1.0f / static_cast<float>(size_);
    for (size_t i = 0; i < size_; ++i) {
        real[i] *= scale;
        imag[i] *= scale;
    }
}

void Fft::BitReversePermute(float* real, float* imag) const {
    for (size_t i = 1, j = 0; i < size_; ++i) {
        size_t bit = size_ >> 1;
        for (; (j & bit) != 0; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
    }
}

void Fft::Transform(float* real, float* imag, bool inverse) const {
    BitReversePermute(real, imag);

    for (size_t blockSize = 2; blockSize <= size_; blockSize <<= 1) {
        const size_t half = blockSize >> 1;
        const size_t twiddleStride = size_ / blockSize;

        for (size_t base = 0; base < size_; base += blockSize) {
            for (size_t j = 0; j < half; ++j) {
                const size_t t = j * twiddleStride;
                const float wr = twiddleCos_[t];
                const float wi = inverse ? -twiddleSin_[t] : twiddleSin_[t];

                const size_t a = base + j;
                const size_t b = a + half;
                const float tr = real[b] * wr - imag[b] * wi;
                const float ti = real[b] * wi + imag[b] * wr;

                real[b] = real[a] - tr;
                imag[b] = imag[a] - ti;
                real[a] += tr;
                imag[a] += ti;
            }
        }
    }
}

} // namespace sudwalfulkaan
