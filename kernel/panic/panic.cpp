#include "panic.hpp"
#include "../console/serial.hpp"
#include "../console/format.hpp"
namespace peregrinus::panic {
[[noreturn]] void stop(const char* reason){
    asm volatile("cli");
    serial::writeln(""); serial::writeln("*** PEREGRINUS KERNEL PANIC ***");
    serial::write("Reason: "); serial::writeln(reason ? reason : "unknown");
    for(;;) asm volatile("hlt");
}
[[noreturn]] void exception(uint64_t vector,uint64_t error,uint64_t rip,uint64_t cs,uint64_t rflags){
    asm volatile("cli");
    char n[24],h[19];
    serial::writeln(""); serial::writeln("*** CPU EXCEPTION ***");
    format::dec64(vector,n); serial::write("Vector: "); serial::writeln(n);
    format::hex64(error,h); serial::write("Error : "); serial::writeln(h);
    format::hex64(rip,h); serial::write("RIP   : "); serial::writeln(h);
    format::hex64(cs,h); serial::write("CS    : "); serial::writeln(h);
    format::hex64(rflags,h); serial::write("RFLAGS: "); serial::writeln(h);
    for(;;) asm volatile("hlt");
}
}
