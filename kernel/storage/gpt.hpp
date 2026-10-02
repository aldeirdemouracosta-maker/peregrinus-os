#pragma once
#include <stdint.h>
#include <stddef.h>

namespace peregrinus::gpt {

struct Guid { uint8_t bytes[16]; };

struct HeaderInfo {
    bool valid_signature;
    bool valid_header_size;
    bool valid_crc;
    uint32_t revision;
    uint32_t header_size;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    Guid disk_guid;
    uint64_t entries_lba;
    uint32_t entry_count;
    uint32_t entry_size;
    uint32_t entries_crc32;
};

enum class PartitionRole : uint8_t { unknown, system, recovery, data };
enum class CopyKind : uint8_t { primary, backup };
enum class Selection : uint8_t { none, primary, backup };

struct PartitionInfo {
    bool used;
    Guid type_guid;
    Guid unique_guid;
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t attributes;
    char name[37];
    PartitionRole role;
};

struct TableInfo {
    bool header_ok;
    bool entries_crc_ok;
    uint32_t declared_entries;
    uint32_t parsed_entries;
    uint32_t used_entries;
    PartitionInfo partitions[16];
};

struct CopyValidation {
    bool header_ok;
    bool layout_ok;
    bool entries_ok;
    bool valid;
};

struct RedundancyInfo {
    bool primary_valid;
    bool backup_valid;
    bool headers_coherent;
    bool tables_match;
    bool split_brain;
    bool degraded;
    Selection selected;
};

HeaderInfo parse_header(const uint8_t* sector, size_t bytes);
TableInfo parse_entries(const HeaderInfo& header, const uint8_t* data, size_t bytes);
CopyValidation validate_copy(const HeaderInfo& header, const TableInfo& table, CopyKind kind, uint64_t device_last_lba, uint32_t logical_sector_bytes);
RedundancyInfo assess_redundancy(const HeaderInfo& primary_header, const TableInfo& primary_table, const CopyValidation& primary,
                                 const HeaderInfo& backup_header, const TableInfo& backup_table, const CopyValidation& backup,
                                 bool entry_arrays_equal);
const char* role_name(PartitionRole role);
const char* selection_name(Selection selection);
bool guid_equal(const Guid& a, const Guid& b);
void guid_text(const Guid& guid, char out[37]);
bool self_test();

}
