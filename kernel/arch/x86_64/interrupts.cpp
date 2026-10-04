#include "interrupts.hpp"
#include <peregrinus/io.hpp>
#include "../../runtime/ring.hpp"
#include "../../runtime/spin.hpp"
#include "../../mm/mmio.hpp"

extern "C" void* peregrinus_irq_table[16];

namespace peregrinus::interrupts {
namespace {
constexpr uint16_t PIC1_CMD = 0x20, PIC1_DATA = 0x21, PIC2_CMD = 0xA0, PIC2_DATA = 0xA1;
constexpr uint16_t PIT_CH0 = 0x40, PIT_CMD = 0x43;
constexpr uint16_t COM1 = 0x3F8, KBD_DATA = 0x60, KBD_STATUS = 0x64;
constexpr uint8_t EOI = 0x20;
constexpr uint32_t PIT_HZ = 1193182;

volatile uint64_t g_ticks = 0;
volatile bool g_active = false;
ByteRing<128> g_kbd;
ByteRing<512> g_ser;

void io_wait() { io::out8(0x80, 0); }
void pic_remap_and_mask(uint8_t mask1, uint8_t mask2) {
    io::out8(PIC1_CMD, 0x11); io_wait(); io::out8(PIC2_CMD, 0x11); io_wait();   // ICW1: init + ICW4
    io::out8(PIC1_DATA, 0x20); io_wait(); io::out8(PIC2_DATA, 0x28); io_wait(); // vector offsets
    io::out8(PIC1_DATA, 0x04); io_wait(); io::out8(PIC2_DATA, 0x02); io_wait(); // cascade on IRQ2
    io::out8(PIC1_DATA, 0x01); io_wait(); io::out8(PIC2_DATA, 0x01); io_wait(); // 8086 mode
    io::out8(PIC1_DATA, mask1); io::out8(PIC2_DATA, mask2);
}
uint64_t rdmsr(uint32_t m) { uint32_t lo, hi; asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(m)); return (uint64_t(hi) << 32) | lo; }
void wrmsr(uint32_t m, uint64_t v) { asm volatile("wrmsr" :: "c"(m), "a"(uint32_t(v)), "d"(uint32_t(v >> 32))); }
// The 8259 output reaches the CPU through the local APIC's LINT0 pin ("virtual wire"). Bootloaders
// may leave the LAPIC enabled with LINT0 masked, which silently blocks every PIC interrupt. Put
// LINT0 in ExtINT mode and LINT1 in NMI mode, enable the LAPIC with spurious vector 0xFF.
bool lapic_virtual_wire() {
    const uint64_t base = rdmsr(0x1B);
    if (!(base & (1u << 11))) return true;  // LAPIC globally disabled: PIC INTR goes straight to the CPU
    constexpr uint32_t EXTINT = 0x700, NMI = 0x400, SW_ENABLE = 0x100, SPURIOUS = 0xFF;
    if (base & (1u << 10)) {  // x2APIC: registers are MSRs 0x800 + offset/16
        wrmsr(0x808, 0);                                   // TPR: accept all priorities
        wrmsr(0x80F, (rdmsr(0x80F) & ~0xFFull) | SW_ENABLE | SPURIOUS);
        wrmsr(0x835, EXTINT); wrmsr(0x836, NMI);
        return true;
    }
    auto* r = static_cast<volatile uint32_t*>(mmio::map(base & 0xFFFFF000ull, 0x1000));
    if (!r) return false;
    r[0x80 / 4] = 0;                                         // TPR
    r[0xF0 / 4] = (r[0xF0 / 4] & ~0xFFu) | SW_ENABLE | SPURIOUS;
    r[0x350 / 4] = EXTINT; r[0x360 / 4] = NMI;
    return true;
}
// In-service register: distinguishes real IRQ7/IRQ15 from spurious ones.
uint8_t pic_isr(uint16_t cmd) { io::out8(cmd, 0x0B); return io::in8(cmd); }
}

extern "C" void peregrinus_irq_dispatch(uint32_t irq) {
    if (irq == 7 && !(pic_isr(PIC1_CMD) & 0x80)) return;                       // spurious: no EOI
    if (irq == 15 && !(pic_isr(PIC2_CMD) & 0x80)) { io::out8(PIC1_CMD, EOI); return; }
    switch (irq) {
        case 0: g_ticks = g_ticks + 1; break;
        case 1:
            // Drain what the controller holds; aux (mouse) bytes are discarded.
            for (int i = 0; i < 16 && (io::in8(KBD_STATUS) & 0x01); ++i) {
                const uint8_t st = io::in8(KBD_STATUS);
                const uint8_t data = io::in8(KBD_DATA);
                if (!(st & 0x20)) g_kbd.push(data);
            }
            break;
        case 4:
            for (int i = 0; i < 64 && (io::in8(COM1 + 5) & 0x01); ++i) g_ser.push(io::in8(COM1));
            break;
        default: break;
    }
    if (irq >= 8) io::out8(PIC2_CMD, EOI);
    io::out8(PIC1_CMD, EOI);
}

bool init(bool keyboard_present, bool serial_present) {
    g_active = false;
    const uint16_t divisor = static_cast<uint16_t>(PIT_HZ / timer_hz);
    io::out8(PIT_CMD, 0x34);  // channel 0, lo/hi, mode 2 (rate generator)
    io::out8(PIT_CH0, static_cast<uint8_t>(divisor)); io::out8(PIT_CH0, static_cast<uint8_t>(divisor >> 8));
    // Unmask IRQ0 (timer), IRQ1 (keyboard, if present), IRQ4 (COM1); slave fully masked.
    const uint8_t mask1 = static_cast<uint8_t>(~((1u << 0) | (keyboard_present ? (1u << 1) : 0u) | (serial_present ? (1u << 4) : 0u)));
    pic_remap_and_mask(mask1, 0xFF);
    if (!lapic_virtual_wire()) { io::out8(PIC1_DATA, 0xFF); io::out8(PIC2_DATA, 0xFF); return false; }
    if (serial_present) io::out8(COM1 + 1, 0x01);  // UART: interrupt on received data
    asm volatile("sti");
    const uint64_t start = g_ticks;
    if (!spin::until([&] { return g_ticks >= start + 2; }, 50000000)) {
        asm volatile("cli");
        io::out8(PIC1_DATA, 0xFF); io::out8(PIC2_DATA, 0xFF);
        if (serial_present) io::out8(COM1 + 1, 0x00);
        return false;
    }
    g_active = true;
    return true;
}
bool active() { return g_active; }
uint64_t ticks() { return g_ticks; }
bool next_scancode(uint8_t& sc) { return g_kbd.pop(sc); }
bool next_serial_byte(uint8_t& b) { return g_ser.pop(b); }
uint64_t dropped_input() { return g_kbd.dropped() + g_ser.dropped(); }
void idle_wait() {
    if (!g_active) { asm volatile("pause"); return; }
    asm volatile("cli");
    if (g_kbd.empty() && g_ser.empty()) asm volatile("sti; hlt" ::: "memory");  // sti shadow: no lost wakeup
    else asm volatile("sti");
}
}
