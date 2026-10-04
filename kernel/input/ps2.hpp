#pragma once
#include <stdint.h>
namespace peregrinus::ps2 {
// i8042 keyboard controller, polled (interrupts stay disabled). Uses whatever the firmware
// configured; it does not reprogram the controller. Returns false when no controller answers
// (port reads 0xFF), e.g. UEFI machines without PS/2 or legacy-USB emulation.
bool init();
bool present();
// Next keyboard scancode, if any. Mouse (aux) bytes are discarded.
bool poll(uint8_t& scancode);
// Asks the i8042 and the chipset (port 0xCF9) for a reset. Returns only if both were ignored.
void request_reset();
}
