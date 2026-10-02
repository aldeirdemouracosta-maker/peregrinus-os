#pragma once
#include <stdint.h>
namespace peregrinus::paging {
struct Status { bool initialized; bool four_level; uint64_t cr3_phys; uint64_t tables_created; uint64_t pages_mapped; };
bool init();
bool map_page(uint64_t virt,uint64_t phys,uint64_t leaf_flags);
bool pml4_slot_unused(unsigned index);
const Status& status();
}
