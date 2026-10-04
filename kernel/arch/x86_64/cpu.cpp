#include <stddef.h>
#include "cpu.hpp"
#include "../../console/serial.hpp"

namespace peregrinus::cpu {
namespace {
char g_hv[13]{};
bool g_hv_ready=false;
bool g_hv_present=false;
// Byte stores: char arrays are not 4-byte aligned objects (no aliasing/alignment UB).
void put32(char* d,unsigned int v){for(int i=0;i<4;++i)d[i]=char(v>>(8*i));}
void detect_hypervisor(){
    if(g_hv_ready)return; g_hv_ready=true;
    unsigned int eax=1,ebx=0,ecx=0,edx=0;
    asm volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    g_hv_present=(ecx&(1u<<31))!=0;
    if(!g_hv_present)return;
    eax=0x40000000u;
    asm volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    put32(&g_hv[0],ebx);put32(&g_hv[4],ecx);put32(&g_hv[8],edx);
    g_hv[12]='\0';
}
bool eq(const char* a,const char* b){for(size_t i=0;;++i){if(a[i]!=b[i])return false;if(a[i]=='\0')return true;}}
}
void report(){
    unsigned int eax=0,ebx=0,ecx=0,edx=0; char vendor[13]{};
    asm volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    put32(&vendor[0],ebx);put32(&vendor[4],edx);put32(&vendor[8],ecx); vendor[12]='\0';
    serial::write("CPU vendor: "); serial::writeln(vendor);
    detect_hypervisor();
    if(g_hv_present){serial::write("Hypervisor: ");serial::writeln(g_hv);}
}
bool hypervisor_present(){detect_hypervisor();return g_hv_present;}
const char* hypervisor_vendor(){detect_hypervisor();return g_hv_present?g_hv:"";}
bool qemu_qualification_environment(){
    detect_hypervisor();
    return g_hv_present&&(eq(g_hv,"TCGTCGTCGTCG")||eq(g_hv,"KVMKVMKVM\0\0\0"));
}
}
