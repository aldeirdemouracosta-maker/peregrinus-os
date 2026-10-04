// Bounded SPSC byte ring used between IRQ handlers and the main loop.
#include <cstdio>
#include "runtime/ring.hpp"
int main() {
    peregrinus::ByteRing<8> r;
    uint8_t b = 0;
    if (r.pop(b) || !r.empty()) { std::puts("FAIL: empty ring"); return 1; }
    for (uint8_t i = 0; i < 8; ++i) if (!r.push(i)) { std::puts("FAIL: push within capacity"); return 2; }
    if (r.push(99) || r.push(100) || r.dropped() != 2) { std::puts("FAIL: overflow must drop and count"); return 3; }
    for (uint8_t i = 0; i < 8; ++i) if (!r.pop(b) || b != i) { std::puts("FAIL: FIFO order"); return 4; }
    // Index wrap-around over many cycles keeps order and never exceeds capacity.
    for (unsigned n = 0; n < 100000; ++n) { if (!r.push(uint8_t(n)) || !r.pop(b) || b != uint8_t(n)) { std::puts("FAIL: wrap"); return 5; } }
    return r.empty() ? 0 : 6;
}
