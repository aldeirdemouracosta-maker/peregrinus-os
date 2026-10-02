#pragma once
#include <stdint.h>
#include "gpt.hpp"
namespace peregrinus::disk_probe {
struct Result {
    bool attempted;
    bool device_identified;
    uint32_t logical_sector_bytes;
    uint64_t device_last_lba;
    bool primary_header_read;
    bool backup_header_read;
    bool primary_valid;
    bool backup_valid;
    bool headers_coherent;
    bool tables_match;
    bool split_brain;
    bool degraded;
    gpt::Selection selected;
    uint32_t used_partitions;
    bool has_system;
    bool has_recovery;
    bool has_data;
    gpt::Guid disk_guid;
};
Result run_qemu_gpt_probe();
const gpt::TableInfo& last_table();
bool has_valid_table();
}
