#pragma once
#include <stdint.h>
namespace peregrinus::format {
void hex64(uint64_t value, char out[19]);
void hex32(uint32_t value, char out[11]);
void dec64(uint64_t value, char out[24]);
}
