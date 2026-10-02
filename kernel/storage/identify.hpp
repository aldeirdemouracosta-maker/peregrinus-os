#pragma once
#include <stdint.h>
#include <stddef.h>

namespace peregrinus::identify {

struct DeviceInfo {
    bool valid;
    bool lba_supported;
    bool lba48_supported;
    bool checksum_present;
    bool checksum_valid;
    uint32_t logical_sector_bytes;
    uint32_t physical_sector_bytes;
    uint64_t sector_count;
    uint64_t last_lba;
    uint64_t capacity_bytes;
    char serial[21];
    char firmware[9];
    char model[41];
};

DeviceInfo parse(const uint8_t* identify512, size_t bytes);
bool self_test();

}
