#include "idt.hpp"
#include "../../console/serial.hpp"
#include "gdt.hpp"
#include <stdint.h>
extern "C" void* peregrinus_isr_table[32];
extern "C" void* peregrinus_irq_table[16];
extern "C" void peregrinus_lapic_spurious();
namespace peregrinus::idt {
struct __attribute__((packed)) Entry { uint16_t off_lo, sel; uint8_t ist, type_attr; uint16_t off_mid; uint32_t off_hi, zero; };
struct __attribute__((packed)) IDTR { uint16_t limit; uint64_t base; };
alignas(16) static Entry table[256]{};
static void set(int v, void(*fn)(), uint8_t ist=0) { uint64_t a=reinterpret_cast<uint64_t>(fn); table[v]={uint16_t(a),0x08,ist,0x8E,uint16_t(a>>16),uint32_t(a>>32),0}; }
void init(){
    for(int i=0;i<32;i++) set(i,reinterpret_cast<void(*)()>(peregrinus_isr_table[i]));
    set(2,reinterpret_cast<void(*)()>(peregrinus_isr_table[2]),gdt::ist_nmi_machine_check);
    set(8,reinterpret_cast<void(*)()>(peregrinus_isr_table[8]),gdt::ist_double_fault);
    set(18,reinterpret_cast<void(*)()>(peregrinus_isr_table[18]),gdt::ist_nmi_machine_check);
    for(int i=0;i<16;i++) set(0x20+i,reinterpret_cast<void(*)()>(peregrinus_irq_table[i]));  // masked until interrupts::init
    set(0xFF,peregrinus_lapic_spurious);
    IDTR idtr{uint16_t(sizeof(table)-1),reinterpret_cast<uint64_t>(&table)};
    asm volatile("lidt %0"::"m"(idtr):"memory");
    serial::writeln("IDT: CPU exception gates installed; #DF/NMI/#MC on IST (PIC IRQ gates 0x20-0x2F installed, masked)");
}
}
