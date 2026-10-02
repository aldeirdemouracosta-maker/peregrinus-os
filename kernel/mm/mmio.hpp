#pragma once
#include <stdint.h>
#include <stddef.h>
namespace peregrinus::mmio {
enum class Cache : uint8_t { uncached };
struct Status { bool initialized; bool pat3_uc; bool nx_enabled; uint64_t window_base; uint64_t next_virtual; uint64_t mappings; };
bool init();
void* map(uint64_t phys,size_t bytes,Cache cache=Cache::uncached);
const Status& status();
}
