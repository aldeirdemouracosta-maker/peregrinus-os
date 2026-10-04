#include "gdt.hpp"
#include "../../console/serial.hpp"
#include <stdint.h>
namespace peregrinus::gdt {
struct __attribute__((packed)) GDTR { uint16_t limit; uint64_t base; };
struct __attribute__((packed)) Tss { uint32_t reserved0; uint64_t rsp[3]; uint64_t reserved1; uint64_t ist[7]; uint64_t reserved2; uint16_t reserved3; uint16_t iomap_base; };
static_assert(sizeof(Tss)==104);
alignas(16) static uint8_t g_ist_df[8192];      // exception handlers only format and print
alignas(16) static uint8_t g_ist_nmi_mc[8192];
alignas(16) static Tss g_tss{};
alignas(8) static uint64_t table[5] = {
    0x0000000000000000ULL,
    0x00AF9A000000FFFFULL,  // 0x08 kernel code
    0x00AF92000000FFFFULL,  // 0x10 kernel data
    0, 0                    // 0x18 64-bit TSS (two slots)
};
void init(){
    g_tss.ist[ist_double_fault-1]=reinterpret_cast<uint64_t>(g_ist_df+sizeof(g_ist_df));
    g_tss.ist[ist_nmi_machine_check-1]=reinterpret_cast<uint64_t>(g_ist_nmi_mc+sizeof(g_ist_nmi_mc));
    g_tss.iomap_base=sizeof(Tss);  // no I/O permission bitmap
    const uint64_t base=reinterpret_cast<uint64_t>(&g_tss),limit=sizeof(Tss)-1;
    table[3]=(limit&0xFFFFu)|((base&0xFFFFFFu)<<16)|(0x89ULL<<40)|(((limit>>16)&0xFu)<<48)|(((base>>24)&0xFFu)<<56);
    table[4]=base>>32;
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
    asm volatile("ltr %w0"::"r"(uint16_t(0x18)):"memory");
    serial::writeln("GDT: loaded; TSS with IST stacks for #DF and NMI/#MC");
}
}
