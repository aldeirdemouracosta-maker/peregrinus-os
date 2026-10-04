// Stack-smashing protector support for -fstack-protector-strong with a global guard
// (-mstack-protector-guard=global; there is no TLS segment in this kernel).
#include <stdint.h>
#include "../panic/panic.hpp"

extern "C" {
uintptr_t __stack_chk_guard = 0x595e9fbd94fda700ULL;
const char* peregrinus_stack_guard_source = "static";

[[noreturn]] void __stack_chk_fail() { peregrinus::panic::stop("stack smashing detected (stack protector)"); }

// Runs from _start before any protected function, on the bootloader stack.
__attribute__((no_stack_protector)) void peregrinus_stack_guard_init() {
    uint32_t a = 1, b = 0, c = 0, d = 0;
    asm volatile("cpuid" : "+a"(a), "=b"(b), "=c"(c), "=d"(d));
    uint64_t seed = 0;
    bool ok = false;
    if (c & (1u << 30)) {  // RDRAND
        for (int i = 0; i < 32 && !ok; ++i) {
            unsigned char cf = 0;
            asm volatile("rdrand %0; setc %1" : "=r"(seed), "=qm"(cf));
            ok = cf != 0;
        }
    }
    if (ok) {
        peregrinus_stack_guard_source = "RDRAND";
    } else {
        uint32_t lo, hi;
        asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
        seed = ((uint64_t(hi) << 32) | lo) * 0x9E3779B97F4A7C15ULL;
        peregrinus_stack_guard_source = "TSC (weak)";
    }
    // Low byte zero: string overflows cannot reproduce the canary.
    __stack_chk_guard ^= seed & ~uint64_t(0xff);
}
}
