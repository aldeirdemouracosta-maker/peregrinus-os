#pragma once
#include <stdint.h>
namespace peregrinus::io {
inline void out8(uint16_t p,uint8_t v){asm volatile("outb %0,%1"::"a"(v),"Nd"(p));}
inline uint8_t in8(uint16_t p){uint8_t v;asm volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
inline void out16(uint16_t p,uint16_t v){asm volatile("outw %0,%1"::"a"(v),"Nd"(p));}
inline uint16_t in16(uint16_t p){uint16_t v;asm volatile("inw %1,%0":"=a"(v):"Nd"(p));return v;}
inline void out32(uint16_t p,uint32_t v){asm volatile("outl %0,%1"::"a"(v),"Nd"(p));}
inline uint32_t in32(uint16_t p){uint32_t v;asm volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}
}
