#include "gdt.hpp"
#include "../../console/serial.hpp"
#include <stdint.h>
namespace peregrinus::gdt {
struct __attribute__((packed)) GDTR { uint16_t limit; uint64_t base; };
alignas(8) static uint64_t table[3] = {
    0x0000000000000000ULL,
    0x00AF9A000000FFFFULL,
    0x00AF92000000FFFFULL
};
void init(){
    GDTR gdtr{uint16_t(sizeof(table)-1), reinterpret_cast<uint64_t>(&table)};
    asm volatile("lgdt %0"::"m"(gdtr):"memory");
    asm volatile(
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%ss\n"
        : : : "rax", "memory");
    serial::writeln("GDT: loaded and segment selectors refreshed");
}
}
