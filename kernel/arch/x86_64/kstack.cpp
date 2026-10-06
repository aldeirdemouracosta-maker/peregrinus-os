#include "kstack.hpp"
#include "../../mm/memory.hpp"
#include "../../mm/paging.hpp"
namespace peregrinus::kstack {
bool create(uint64_t& top){
    top=0;
    if(!paging::status().initialized||!paging::pml4_slot_unused(509))return false;
    uint32_t lo,hi;asm volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0xC0000080u));
    const uint64_t nx=(lo&(1u<<11))?(1ULL<<63):0;
    // Page 0 of the window is the guard: never mapped.
    for(unsigned i=1;i<=stack_pages;++i){
        void* phys=memory::alloc_page();if(!phys)return false;
        uint64_t* v=static_cast<uint64_t*>(memory::phys_to_hhdm(reinterpret_cast<uint64_t>(phys)));if(!v)return false;
        for(unsigned j=0;j<512;++j)v[j]=0;
        if(!paging::map_page(window_base+uint64_t(i)*4096u,reinterpret_cast<uint64_t>(phys),(1ULL<<1)|nx))return false;
    }
    top=window_base+uint64_t(stack_pages+1)*4096u;
    return true;
}
}
