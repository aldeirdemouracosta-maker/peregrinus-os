#pragma once
#include <stdint.h>
struct limine_memmap_response;
namespace peregrinus::memory {
struct Summary {
    uint64_t usable_bytes;
    uint64_t total_described_bytes;
    uint64_t entries;
    uint64_t largest_usable_bytes;
    uint64_t dma32_reserved_bytes;
};
Summary init_from_limine(limine_memmap_response* map);
const Summary& summary();
void set_hhdm_offset(uint64_t offset);
uint64_t hhdm_offset();
void* phys_to_hhdm(uint64_t phys);
void init_page_allocator(limine_memmap_response* map);
void* alloc_page();
// Physically contiguous run of whole pages from the page allocator (bump: consecutive pages are
// contiguous). Returns the physical base, or nullptr if not enough memory remains.
void* alloc_contiguous(uint64_t bytes);
void* alloc_dma32_page();
uint64_t free_pages_estimate();
uint64_t dma32_free_pages_estimate();
}
