#include "mmio.hpp"
#include "paging.hpp"
#include "../console/serial.hpp"
#include "../console/format.hpp"

namespace peregrinus::mmio {
namespace {
static Status g{};
static constexpr uint64_t PAGE=4096;
static constexpr uint64_t WINDOW_BASE=0xffffff0000000000ULL; // PML4 slot 510, reserved by Peregrinus.
static constexpr uint64_t WINDOW_SIZE=1ULL<<30; // 1 GiB bring-up window.
static constexpr uint64_t RW=1ULL<<1,PWT=1ULL<<3,PCD=1ULL<<4,NX=1ULL<<63;
static uint64_t rdmsr(uint32_t msr){uint32_t lo,hi;asm volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(msr));return (uint64_t(hi)<<32)|lo;}
static uint64_t align_down(uint64_t v){return v&~(PAGE-1);}
static uint64_t align_up(uint64_t v){return (v+PAGE-1)&~(PAGE-1);}
}

bool init(){
    g={}; g.window_base=WINDOW_BASE; g.next_virtual=WINDOW_BASE;
    if(!paging::status().initialized){serial::writeln("MMIO: paging mapper unavailable");return false;}
    if(!paging::pml4_slot_unused(510)){serial::writeln("MMIO: reserved PML4 slot 510 already in use; mapping blocked");return false;}
    uint32_t eax,ebx,ecx,edx; eax=1; asm volatile("cpuid":"=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx):"a"(eax),"c"(0));
    if((edx&(1u<<16))==0){serial::writeln("MMIO: CPU PAT unavailable; mapping blocked");return false;}
    const uint64_t pat=rdmsr(0x277); const uint8_t pat3=uint8_t((pat>>(3*8))&0xFFu);
    g.pat3_uc=(pat3==0x00); // Intel memory type 0 = UC.
    const uint64_t efer=rdmsr(0xC0000080); g.nx_enabled=(efer&(1ULL<<11))!=0;
    if(!g.pat3_uc){serial::writeln("MMIO: PAT3 is not UC; mapping blocked");return false;}
    g.initialized=true; serial::writeln("MMIO: dedicated UC window READY (PML4 slot 510)"); return true;
}

void* map(uint64_t phys,size_t bytes,Cache){
    if(!g.initialized||bytes==0)return nullptr;
    const uint64_t pbase=align_down(phys),off=phys-pbase,span=align_up(off+bytes);
    uint64_t vbase=align_up(g.next_virtual);
    if(vbase<WINDOW_BASE || span>WINDOW_SIZE || vbase-WINDOW_BASE>WINDOW_SIZE-span) return nullptr;
    const uint64_t flags=RW|PWT|PCD|(g.nx_enabled?NX:0); // PAT index 3 => UC, NX prevents execution from device memory.
    for(uint64_t x=0;x<span;x+=PAGE) if(!paging::map_page(vbase+x,pbase+x,flags)) return nullptr;
    g.next_virtual=vbase+span; ++g.mappings;
    char h[19];format::hex64(phys,h);serial::write("MMIO mapped phys ");serial::write(h);format::hex64(vbase+off,h);serial::write(" -> virt ");serial::writeln(h);
    return reinterpret_cast<void*>(static_cast<uintptr_t>(vbase+off));
}
const Status& status(){return g;}
}
