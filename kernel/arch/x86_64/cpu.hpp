#pragma once
namespace peregrinus::cpu {
void report();
bool hypervisor_present();
bool qemu_qualification_environment();
const char* hypervisor_vendor();
// Turns on SSE (CR0.EM=0, CR0.MP=1, CR4.OSFXSR/OSXMMEXCPT, default MXCSR). Only the local-LLM
// profile calls it; the rest of the kernel is built with -mgeneral-regs-only. Interrupt handlers
// never touch SSE registers, so no extended state needs saving.
void enable_sse();
}
