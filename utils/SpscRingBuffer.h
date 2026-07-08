#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace ninetysix {

/**
 * @brief Lock-free single-producer / single-consumer float ring buffer.
 *
 * Designed for handing audio between an interrupt context and a main loop:
 * exactly one thread may call Write() and exactly one thread may call Read().
 * Storage is caller-owned so it can be placed in SDRAM. Capacity must be a
 * power of two. Indices are 32-bit and wrap-safe; Write() and Read() perform
 * no allocation and never block.
 */
class SpscRingBuffer {
public:
    /**
     * @brief Attach caller-owned storage and reset the buffer to empty.
     * @param storage Caller-owned array of `capacity` floats.
     * @param capacity Number of floats; must be a power of two >= 2.
     * @return true when storage is non-null and capacity is valid.
     */
    bool Init(float* storage, size_t capacity);

    /**
     * @brief Producer side: copy up to `count` samples into the buffer.
     * @return Number of samples actually written (limited by free space).
     */
    size_t Write(const float* source, size_t count);

    /**
     * @brief Consumer side: copy up to `count` samples out of the buffer.
     * @return Number of samples actually read (limited by fill level).
     */
    size_t Read(float* destination, size_t count);

    /** @brief Samples available to the consumer. */
    size_t ReadAvailable() const;

    /** @brief Free space available to the producer. */
    size_t WriteAvailable() const;

    size_t Capacity() const { return capacity_; }

private:
    float* storage_ = nullptr;
    size_t capacity_ = 0;
    uint32_t mask_ = 0;
    std::atomic<uint32_t> writeIndex_{0};
    std::atomic<uint32_t> readIndex_{0};
};

} // namespace ninetysix
