#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace core {

template <typename T, size_t Capacity>
class RingBuffer {
public:
    bool empty() const { return count_ == 0; }
    bool full() const { return count_ == Capacity; }
    size_t size() const { return count_; }
    constexpr size_t capacity() const { return Capacity; }
    uint32_t dropped() const { return dropped_; }

    void push(const T& item) {
        if (full()) {
            discardOldest();
            ++dropped_;
        }
        items_[(head_ + count_) % Capacity] = item;
        ++count_;
    }

    const T& front() const { return items_[head_]; }

    void pop() {
        if (empty()) return;
        discardOldest();
    }

private:
    void discardOldest() {
        head_ = (head_ + 1) % Capacity;
        --count_;
    }

    std::array<T, Capacity> items_{};
    size_t head_ = 0;
    size_t count_ = 0;
    uint32_t dropped_ = 0;
};

}
