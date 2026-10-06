#pragma once
#include <stdint.h>
namespace peregrinus::interrupts {
inline constexpr uint32_t timer_hz = 100;
// Remaps the 8259 PIC to vectors 0x20-0x2F, programs the PIT at timer_hz and unmasks only the
// timer (IRQ0), keyboard (IRQ1) and COM1 (IRQ4) when those devices exist. Then checks that timer ticks really arrive;
// if they do not, everything is masked again and the kernel stays in polling mode (fail-closed:
// input keeps working, only idle power saving is lost). Returns true when interrupts are live.
bool init(bool keyboard_present, bool serial_present);
bool active();
uint64_t ticks();
// Received bytes queued by the IRQ handlers (bounded rings; overflow drops and counts).
bool next_scancode(uint8_t& sc);
bool next_serial_byte(uint8_t& b);
uint64_t dropped_input();
// Sleep until the next interrupt if no input is queued (race-free cli/check/sti;hlt).
void idle_wait();
}
