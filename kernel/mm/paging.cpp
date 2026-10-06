#include "paging.hpp"
#include "memory.hpp"
#include "../console/serial.hpp"
#include "../console/format.hpp"

namespace peregrinus::paging {
namespace {
static Status g{};
static constexpr uint64_t P=1ULL<<0,RW=1ULL<<1,PS=1ULL<<7;
static constexpr uint64_t ADDR_MASK=0x000FFFFFFFFFF000ULL;
static uint64_t read_cr3(){uint64_t v;asm volatile("mov %%cr3,%0":"=r"(v));return v;}
static uint64_t read_cr4(){uint64_t v;asm volatile("mov %%cr4,%0":"=r"(v));return v;}
static void invlpg(uint64_t v){asm volatile("invlpg (%0)"::"r"(v):"memory");}
static void zero_page(uint64_t phys){auto* p=(uint64_t*)memory::phys_to_hhdm(phys);if(!p)return;for(unsigned i=0;i<512;++i)p[i]=0;}
static uint64_t* table_virt(uint64_t phys){return reinterpret_cast<uint64_t*>(memory::phys_to_hhdm(phys));}
static uint64_t* ensure_next(uint64_t* table,unsigned idx){
    uint64_t e=table[idx];
    if(e&P){if(e&PS)return nullptr;return table_virt(e&ADDR_MASK);}
    uint64_t phys=reinterpret_cast<uint64_t>(memory::alloc_page()); if(!phys)return nullptr;
    zero_page(phys); table[idx]=(phys&ADDR_MASK)|P|RW; ++g.tables_created;
    return table_virt(phys);
}
}

bool init(){
    g={};
    if(!memory::hhdm_offset()){serial::writeln("Paging: HHDM unavailable");return false;}
    if(read_cr4()&(1ULL<<12)){serial::writeln("Paging: LA57 detected; 5-level mapper deferred");return false;}
    g.four_level=true; g.cr3_phys=read_cr3()&ADDR_MASK; g.initialized=table_virt(g.cr3_phys)!=nullptr;
    char h[19];format::hex64(g.cr3_phys,h);serial::write("Paging CR3: ");serial::writeln(h);
    serial::writeln(g.initialized?"Paging: Peregrinus 4-level mapper READY":"Paging: mapper FAILED");
    return g.initialized;
}

bool map_page(uint64_t virt,uint64_t phys,uint64_t leaf_flags){
    if(!g.initialized || (virt&0xFFF) || (phys&0xFFF)) return false;
    auto* pml4=table_virt(g.cr3_phys);if(!pml4)return false;
    const unsigned i4=(virt>>39)&0x1FF,i3=(virt>>30)&0x1FF,i2=(virt>>21)&0x1FF,i1=(virt>>12)&0x1FF;
    auto* pdpt=ensure_next(pml4,i4);if(!pdpt)return false;
    auto* pd=ensure_next(pdpt,i3);if(!pd)return false;
    auto* pt=ensure_next(pd,i2);if(!pt)return false;
    if(pt[i1]&P) return false;
    pt[i1]=(phys&ADDR_MASK)|leaf_flags|P;
    invlpg(virt); ++g.pages_mapped; return true;
}
bool unmap_page(uint64_t virt){
    if(!g.initialized||(virt&0xFFF))return false;
    uint64_t* t=table_virt(g.cr3_phys);
    const unsigned idx[3]={unsigned((virt>>39)&0x1FF),unsigned((virt>>30)&0x1FF),unsigned((virt>>21)&0x1FF)};
    for(unsigned l=0;l<3;++l){if(!t)return false;const uint64_t e=t[idx[l]];if(!(e&P)||(e&PS))return false;t=table_virt(e&ADDR_MASK);}
    if(!t)return false;const unsigned i1=(virt>>12)&0x1FF;if(!(t[i1]&P))return false;
    t[i1]=0;invlpg(virt);--g.pages_mapped;return true;
}
bool pml4_slot_unused(unsigned index){
    if(!g.initialized||index>=512)return false;
    auto* pml4=table_virt(g.cr3_phys);return pml4 && (pml4[index]&P)==0;
}
const Status& status(){return g;}
}
