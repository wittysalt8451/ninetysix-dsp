#pragma once

#include <cstddef>

namespace ninetysix {

/**
 * @brief In-place radix-2 complex FFT with caller-owned twiddle tables.
 *
 * Hardware-agnostic and allocation-free: the caller provides all storage,
 * so tables can live in SDRAM on embedded targets. Init() fills the twiddle
 * tables (uses libm, not real-time safe); Forward() and Inverse() perform
 * no allocation and are safe to call from a low-priority processing loop.
 */
class Fft {
public:
    /**
     * @brief Prepare a transform of `size` points.
     * @param size Transform length; must be a power of two >= kMinSize.
     * @param twiddleCos Caller-owned table of size / 2 floats, filled by Init.
     * @param twiddleSin Caller-owned table of size / 2 floats, filled by Init.
     * @return true when size is supported and both tables are non-null.
     */
    bool Init(size_t size, float* twiddleCos, float* twiddleSin);

    /** @brief In-place forward transform (unnormalized). */
    void Forward(float* real, float* imag) const;

    /** @brief In-place inverse transform, scaled by 1 / size. */
    void Inverse(float* real, float* imag) const;

    size_t Size() const { return size_; }

    static bool IsPowerOfTwo(size_t value);

    static constexpr size_t kMinSize = 4;

private:
    void Transform(float* real, float* imag, bool inverse) const;
    void BitReversePermute(float* real, float* imag) const;

    size_t size_ = 0;
    const float* twiddleCos_ = nullptr;
    const float* twiddleSin_ = nullptr;
};

} // namespace ninetysix
