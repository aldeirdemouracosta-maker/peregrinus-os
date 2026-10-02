#include "idt.hpp"
#include "../../console/serial.hpp"
#include <stdint.h>
extern "C" void* peregrinus_isr_table[32];
namespace peregrinus::idt {
struct __attribute__((packed)) Entry { uint16_t off_lo, sel; uint8_t ist, type_attr; uint16_t off_mid; uint32_t off_hi, zero; };
struct __attribute__((packed)) IDTR { uint16_t limit; uint64_t base; };
alignas(16) static Entry table[256]{};
static void set(int v, void(*fn)()) { uint64_t a=reinterpret_cast<uint64_t>(fn); table[v]={uint16_t(a),0x08,0,0x8E,uint16_t(a>>16),uint32_t(a>>32),0}; }
void init(){
    for(int i=0;i<32;i++) set(i,reinterpret_cast<void(*)()>(peregrinus_isr_table[i]));
    IDTR idtr{uint16_t(sizeof(table)-1),reinterpret_cast<uint64_t>(&table)};
    asm volatile("lidt %0"::"m"(idtr):"memory");
    serial::writeln("IDT: CPU exception gates installed (IRQs remain disabled)");
}
}
