#pragma once
#include <stdint.h>
#include "gpt.hpp"

namespace peregrinus::volume {

enum class Role : uint8_t { unknown, system, recovery, data };

struct Volume {
    bool present;
    bool readonly;
    bool bound;
    Role role;
    uint32_t device_index;
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t sector_count;
    gpt::Guid type_guid;
    gpt::Guid unique_guid;
    char name[37];
};

struct Catalog {
    Volume volumes[16];
    uint32_t count;
    int32_t system_index;
    int32_t recovery_index;
    int32_t data_index;
    bool duplicate_role;
    bool readonly_policy;
};

void init();
bool bind_gpt(uint32_t device_index, const gpt::TableInfo& table);
const Catalog& catalog();
const Volume* by_role(Role role);
bool read_sector(const Volume& v, uint64_t relative_lba, void* out512);
const char* role_name(Role role);
bool self_test();

}
