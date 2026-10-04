#include "serial.hpp"
#include <peregrinus/io.hpp>
#include "../runtime/spin.hpp"

namespace peregrinus::serial {
static constexpr unsigned short COM1 = 0x3F8;

bool init() {
    using namespace peregrinus::io;
    out8(COM1 + 1, 0x00);
    out8(COM1 + 3, 0x80);
    out8(COM1 + 0, 0x03);
    out8(COM1 + 1, 0x00);
    out8(COM1 + 3, 0x03);
    out8(COM1 + 2, 0xC7);
    out8(COM1 + 4, 0x0B);
    return true;
}

static bool tx_ready() {
    return (peregrinus::io::in8(COM1 + 5) & 0x20) != 0;
}

void putc(char c) {
    // Bounded wait: a missing or wedged UART must not hang the kernel. On timeout the
    // character is dropped (the console is diagnostic only).
    if (!spin::until([] { return tx_ready(); }, 100000)) return;
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
