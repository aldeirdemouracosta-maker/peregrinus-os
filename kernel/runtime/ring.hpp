#pragma once
#include <stddef.h>
#include <stdint.h>
namespace peregrinus {
// Single-producer (interrupt handler) / single-consumer (main loop) byte ring of fixed size.
// When full, new bytes are dropped and counted (bounded memory, no blocking in IRQ context).
template<size_t N> class ByteRing {
    static_assert(N >= 2 && (N & (N - 1)) == 0, "power of two");
public:
    bool push(uint8_t b) {
        const size_t h = head_, t = tail_;
        if (h - t >= N) { dropped_ = dropped_ + 1; return false; }
        data_[h & (N - 1)] = b;
        asm volatile("" ::: "memory");
        head_ = h + 1;
        return true;
    }
    bool pop(uint8_t& b) {
        const size_t t = tail_;
        if (t == head_) return false;
        asm volatile("" ::: "memory");
        b = data_[t & (N - 1)];
        tail_ = t + 1;
        return true;
    }
    bool empty() const { return tail_ == head_; }
    uint64_t dropped() const { return dropped_; }
private:
    volatile uint8_t data_[N]{};
    volatile size_t head_ = 0, tail_ = 0;
    volatile uint64_t dropped_ = 0;
};
}
