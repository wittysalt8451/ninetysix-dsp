#pragma once

#include <cstddef>
#include <cstdint>

namespace ninetysix {

/**
 * @brief Statically sized fractional delay line, drop-in for the
 * daisysp::DelayLine subset used in this library.
 *
 * Indexing and interpolation mirror DaisySP exactly (decrementing write
 * pointer, linear interpolation between the two samples around the delay
 * set via SetDelay), so replacing it does not change the sound.
 */
template <size_t max_size>
class DelayLine {
public:
    void Init() {
        for (size_t i = 0; i < max_size; i++) line_[i] = 0.0f;
        write_ptr_ = 0;
        delay_ = 0;
        frac_ = 0.0f;
    }

    void SetDelay(float delay) {
        if (delay < 0.0f) delay = 0.0f;
        const int32_t delay_integral = static_cast<int32_t>(delay);
        frac_ = delay - static_cast<float>(delay_integral);
        delay_ = static_cast<size_t>(delay_integral) < max_size
                     ? static_cast<size_t>(delay_integral)
                     : max_size - 1;
    }

    /** Read at the delay set via SetDelay, linearly interpolated */
    float Read() const {
        const float a = line_[(write_ptr_ + delay_) % max_size];
        const float b = line_[(write_ptr_ + delay_ + 1) % max_size];
        return a + (b - a) * frac_;
    }

    void Write(float sample) {
        line_[write_ptr_] = sample;
        write_ptr_ = (write_ptr_ - 1 + max_size) % max_size;
    }

private:
    float line_[max_size];
    size_t write_ptr_ = 0;
    size_t delay_ = 0;
    float frac_ = 0.0f;
};

} // namespace ninetysix
