#pragma once
#include <stdint.h>
namespace peregrinus::kstack {
// Kernel stack in its own virtual window (PML4 slot 509) with an unmapped guard page below it:
// an overflow faults on the guard page (#PF -> #DF on IST) instead of silently corrupting
// whatever memory happened to sit under the bootloader-provided stack.
inline constexpr uint64_t window_base=0xfffffe8000000000ULL;
inline constexpr unsigned stack_pages=16;   // 64 KiB
bool create(uint64_t& top);
}
extern "C" [[noreturn]] void peregrinus_switch_stack(uint64_t top,void(*entry)());
