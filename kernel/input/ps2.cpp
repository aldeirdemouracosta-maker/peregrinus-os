#include "ps2.hpp"
#include <peregrinus/io.hpp>
#include "../runtime/spin.hpp"
namespace peregrinus::ps2 {
namespace {
constexpr uint16_t DATA = 0x60, STATUS = 0x64;
constexpr uint8_t ST_OUTPUT_FULL = 1u << 0, ST_INPUT_FULL = 1u << 1, ST_AUX = 1u << 5;
bool g_present = false;
}
bool init() {
    g_present = false;
    if (io::in8(STATUS) == 0xFF) return false;  // floating bus: no controller
    for (int i = 0; i < 64 && (io::in8(STATUS) & ST_OUTPUT_FULL); ++i) (void)io::in8(DATA);  // drop stale bytes
    g_present = true;
    return true;
}
bool present() { return g_present; }
bool poll(uint8_t& scancode) {
    if (!g_present) return false;
    const uint8_t st = io::in8(STATUS);
    if (!(st & ST_OUTPUT_FULL)) return false;
    const uint8_t data = io::in8(DATA);
    if (st & ST_AUX) return false;
    scancode = data;
    return true;
}
void request_reset() {
    if (g_present && spin::until([] { return (io::in8(STATUS) & ST_INPUT_FULL) == 0; }, 100000)) io::out8(STATUS, 0xFE);
    (void)spin::until([] { return false; }, 1000000);  // bounded delay before the chipset fallback
    io::out8(0xCF9, 0x02); io::out8(0xCF9, 0x06);  // PCI reset control: hard reset
}
}
