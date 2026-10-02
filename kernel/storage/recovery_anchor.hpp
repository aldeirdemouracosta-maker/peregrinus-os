#pragma once
#include <stdint.h>
#include <stddef.h>
#include "gpt.hpp"

namespace peregrinus::recovery_anchor {

inline constexpr uint32_t format_version = 1;
inline constexpr uint32_t flag_known_good = 1u << 0;
inline constexpr uint32_t flag_clean_shutdown = 1u << 1;

enum class Copy : uint8_t { none, first, last };

struct AnchorInfo {
    bool valid_magic;
    bool valid_version;
    bool valid_header_size;
    bool valid_crc;
    bool known_good;
    bool clean_shutdown;
    uint64_t generation;
    gpt::Guid disk_guid;
    gpt::Guid recovery_guid;
    gpt::Guid system_guid;
    gpt::Guid data_guid;
    uint64_t system_first_lba;
    uint64_t system_last_lba;
    uint64_t recovery_first_lba;
    uint64_t recovery_last_lba;
    uint64_t data_first_lba;
    uint64_t data_last_lba;
    char label[33];
};

struct Assessment {
    bool first_valid;
    bool last_valid;
    bool identities_match;
    bool geometry_match;
    bool split_brain;
    bool degraded;
    bool rolling_update;
    bool last_known_good;
    bool selected_clean_shutdown;
    Copy selected;
    uint64_t selected_generation;
};

AnchorInfo parse(const uint8_t* sector, size_t bytes);
Assessment assess(const AnchorInfo& first, bool first_layout_ok,
                  const AnchorInfo& last, bool last_layout_ok);
const char* copy_name(Copy copy);
bool self_test();

}
