#include "serial.hpp"
#include <peregrinus/io.hpp>
#include "../runtime/spin.hpp"

namespace peregrinus::serial {
static constexpr unsigned short COM1 = 0x3F8;
static void (*g_mirror)(char) = nullptr;

void set_mirror(void (*mirror)(char)) { g_mirror = mirror; }

static bool g_present = false;

bool init() {
    using namespace peregrinus::io;
    // Presence check through the scratch register: a missing UART reads back 0xFF/garbage.
    g_present = false;
    out8(COM1 + 7, 0xA5);
    if (in8(COM1 + 7) != 0xA5) return false;
    out8(COM1 + 7, 0x5A);
    if (in8(COM1 + 7) != 0x5A) return false;
    out8(COM1 + 1, 0x00);
    out8(COM1 + 3, 0x80);
    out8(COM1 + 0, 0x03);
    out8(COM1 + 1, 0x00);
    out8(COM1 + 3, 0x03);
    out8(COM1 + 2, 0xC7);
    out8(COM1 + 4, 0x0B);
    g_present = true;
    return true;
}
bool present() { return g_present; }

static bool tx_ready() {
    return (peregrinus::io::in8(COM1 + 5) & 0x20) != 0;
}

bool poll_input(char& c) {
    if (!g_present || (peregrinus::io::in8(COM1 + 5) & 0x01) == 0) return false;
    c = static_cast<char>(peregrinus::io::in8(COM1));
    return true;
}

void putc(char c) {
    if (g_mirror) g_mirror(c);
    if (!g_present) return;
    // Generous bound: the serial line is also a command channel, so a slow consumer must not
    // lose bytes. A UART that stays busy that long is considered wedged and is switched off
    // (fail-closed: one timeout, never a timeout per character).
    if (!spin::until([] { return tx_ready(); }, 20000000)) { g_present = false; return; }
    peregrinus::io::out8(COM1, static_cast<unsigned char>(c));
}

void write(const char* s) {
    while (*s) {
        if (*s == '\n') putc('\r');
        putc(*s++);
    }
}

void writeln(const char* s) {
    write(s);
    write("\n");
}
}
