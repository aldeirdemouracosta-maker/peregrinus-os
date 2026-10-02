#pragma once
#include <stdint.h>
namespace peregrinus::panic {
[[noreturn]] void stop(const char* reason);
[[noreturn]] void exception(uint64_t vector, uint64_t error, uint64_t rip, uint64_t cs, uint64_t rflags);
}
