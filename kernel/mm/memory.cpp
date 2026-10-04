#include "memory.hpp"
#include "../../third_party/limine/limine_min.h"
#include "../console/serial.hpp"
#include "../console/format.hpp"
#include "../config/features.hpp"

namespace peregrinus::memory {
static Summary g{};
static uint64_t bump_cur=0,bump_end=0;
static uint64_t dma_cur=0,dma_end=0;
static uint64_t g_hhdm=0;
static constexpr uint64_t PAGE=4096;
static constexpr uint64_t DMA32_LIMIT=0x100000000ULL;
static constexpr uint64_t DMA32_RESERVE=2ULL*1024*1024;
static uint64_t align_up(uint64_t v,uint64_t a){return (v+a-1)&~(a-1);}
static uint64_t align_down(uint64_t v,uint64_t a){return v&~(a-1);}

Summary init_from_limine(limine_memmap_response* map){
    g={};
    if(!map){serial::writeln("Memory: Limine memory map unavailable");return g;}
    g.entries=map->entry_count;
    for(uint64_t i=0;i<map->entry_count;++i){
        auto* e=map->entries[i]; if(!e) continue;
        g.total_described_bytes+=e->length;
        if(e->type==LIMINE_MEMMAP_USABLE){
            g.usable_bytes+=e->length;
            if(e->length>g.largest_usable_bytes) g.largest_usable_bytes=e->length;
        }
    }
    char n[24]; format::dec64(g.entries,n); serial::write("Memory map entries: "); serial::writeln(n);
    format::dec64(g.usable_bytes/(1024*1024),n); serial::write("Usable memory MiB: "); serial::writeln(n);
    return g;
}

void set_hhdm_offset(uint64_t offset){g_hhdm=offset;}
uint64_t hhdm_offset(){return g_hhdm;}
void* phys_to_hhdm(uint64_t phys){return g_hhdm?reinterpret_cast<void*>(static_cast<uintptr_t>(phys+g_hhdm)):nullptr;}

void init_page_allocator(limine_memmap_response* map){
    bump_cur=bump_end=dma_cur=dma_end=0;
    if(!map) return;

    if(features::dma32_required){
        uint64_t dma_best_base=0,dma_best_len=0;
        for(uint64_t i=0;i<map->entry_count;++i){
            auto* e=map->entries[i]; if(!e||e->type!=LIMINE_MEMMAP_USABLE) continue;
            uint64_t start=align_up(e->base,PAGE); if(start<PAGE) start=PAGE;
            uint64_t raw_end=e->base+e->length;
            uint64_t end=align_down(raw_end<DMA32_LIMIT?raw_end:DMA32_LIMIT,PAGE);
            if(end>start && end-start>dma_best_len){dma_best_base=start;dma_best_len=end-start;}
        }
        if(dma_best_len>=PAGE*16){
            uint64_t reserve=dma_best_len<DMA32_RESERVE?dma_best_len:DMA32_RESERVE;
            reserve=align_down(reserve,PAGE);
            dma_cur=dma_best_base; dma_end=dma_best_base+reserve; g.dma32_reserved_bytes=reserve;
        }
    }

    uint64_t best_base=0,best_len=0;
    for(uint64_t i=0;i<map->entry_count;++i){
        auto* e=map->entries[i]; if(!e||e->type!=LIMINE_MEMMAP_USABLE) continue;
        uint64_t start=align_up(e->base,PAGE),end=align_down(e->base+e->length,PAGE); if(start<PAGE) start=PAGE;
        if(end<=start) continue;
        if(dma_end>dma_cur && start<dma_end && end>dma_cur){
            if(start<dma_cur && dma_cur-start>best_len){best_base=start;best_len=dma_cur-start;}
            if(end>dma_end && end-dma_end>best_len){best_base=dma_end;best_len=end-dma_end;}
        } else if(end-start>best_len){best_base=start;best_len=end-start;}
    }
    bump_cur=best_base; bump_end=best_base+best_len;
    char h[19],n[24];
    format::hex64(bump_cur,h); serial::write("PMM arena base: "); serial::writeln(h);
    format::dec64(free_pages_estimate(),n); serial::write("PMM free pages: "); serial::writeln(n);
    format::dec64(dma32_free_pages_estimate(),n); serial::write("DMA32 reserved pages: "); serial::writeln(n);
}

void* alloc_page(){if(!bump_cur||bump_cur+PAGE>bump_end)return nullptr;uint64_t p=bump_cur;bump_cur+=PAGE;return reinterpret_cast<void*>(p);}
void* alloc_contiguous(uint64_t bytes){const uint64_t span=align_up(bytes,PAGE);if(!bump_cur||span==0||span>bump_end-bump_cur)return nullptr;const uint64_t p=bump_cur;bump_cur+=span;return reinterpret_cast<void*>(p);}
void* alloc_dma32_page(){if(!dma_cur||dma_cur+PAGE>dma_end)return nullptr;uint64_t p=dma_cur;dma_cur+=PAGE;return reinterpret_cast<void*>(p);}
uint64_t free_pages_estimate(){return bump_end>bump_cur?(bump_end-bump_cur)/PAGE:0;}
uint64_t dma32_free_pages_estimate(){return dma_end>dma_cur?(dma_end-dma_cur)/PAGE:0;}
const Summary& summary(){return g;}
}
