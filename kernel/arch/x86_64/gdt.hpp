#pragma once
#include <stdint.h>
namespace peregrinus::gdt {
// Interrupt Stack Table slots. Faults that may arrive on a broken kernel stack (#DF) or at any
// instant (NMI, #MC) run on their own known-good stacks.
inline constexpr uint8_t ist_double_fault=1;
inline constexpr uint8_t ist_nmi_machine_check=2;
void init();
}
