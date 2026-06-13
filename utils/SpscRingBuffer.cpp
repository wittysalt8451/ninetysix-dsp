#include "utils/SpscRingBuffer.h"

namespace sudwalfulkaan {

namespace {
constexpr size_t kMinCapacity = 2;

bool IsPowerOfTwo(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}
} // namespace

bool SpscRingBuffer::Init(float* storage, size_t capacity) {
    if (storage == nullptr || capacity < kMinCapacity || !IsPowerOfTwo(capacity)) {
        return false;
    }
    storage_ = storage;
    capacity_ = capacity;
    mask_ = static_cast<uint32_t>(capacity - 1);
    writeIndex_.store(0, std::memory_order_relaxed);
    readIndex_.store(0, std::memory_order_relaxed);
    return true;
}

size_t SpscRingBuffer::ReadAvailable() const {
    // Unsigned subtraction stays correct across index wrap-around.
    return writeIndex_.load(std::memory_order_acquire)
         - readIndex_.load(std::memory_order_acquire);
}

size_t SpscRingBuffer::WriteAvailable() const {
    return capacity_ - ReadAvailable();
}

size_t SpscRingBuffer::Write(const float* source, size_t count) {
    const uint32_t write = writeIndex_.load(std::memory_order_relaxed);
    const uint32_t read = readIndex_.load(std::memory_order_acquire);
    const size_t free = capacity_ - static_cast<size_t>(write - read);
    const size_t toWrite = count < free ? count : free;

    for (size_t i = 0; i < toWrite; ++i) {
        storage_[(write + i) & mask_] = source[i];
    }

    writeIndex_.store(write + static_cast<uint32_t>(toWrite),
                      std::memory_order_release);
    return toWrite;
}

size_t SpscRingBuffer::Read(float* destination, size_t count) {
    const uint32_t read = readIndex_.load(std::memory_order_relaxed);
    const uint32_t write = writeIndex_.load(std::memory_order_acquire);
    const size_t filled = static_cast<size_t>(write - read);
    const size_t toRead = count < filled ? count : filled;

    for (size_t i = 0; i < toRead; ++i) {
        destination[i] = storage_[(read + i) & mask_];
    }

    readIndex_.store(read + static_cast<uint32_t>(toRead),
                     std::memory_order_release);
    return toRead;
}

} // namespace sudwalfulkaan
