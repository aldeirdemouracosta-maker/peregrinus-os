#pragma once
#include <stdint.h>
#include <peregrinus/recovery_journal.h>
#include "gpt.hpp"
#include "volume.hpp"
namespace peregrinus::recovery_journal {
enum class Copy:uint8_t{none,a,b};
struct Assessment{bool a_valid,b_valid,split_brain;Copy selected; peregrinus_recovery_journal record;};
Assessment assess(const void* a512,const void* b512,const gpt::Guid& disk_guid,const volume::Volume& recovery);
Copy inactive_copy(Copy active);
bool self_test();
const char* copy_name(Copy c);
}
