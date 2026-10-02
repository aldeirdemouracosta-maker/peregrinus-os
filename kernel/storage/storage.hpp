#pragma once
#include <stdint.h>
#include <stddef.h>
#include "identify.hpp"

namespace peregrinus::storage {

enum class DeviceKind : uint8_t { none, ahci_sata };

struct Device {
    DeviceKind kind;
    uint8_t controller_index;
    uint8_t port_index;
    bool readonly;
    bool present;
    bool identified;
    identify::DeviceInfo info;
};

struct Inventory { Device devices[8]; uint32_t count; };

void init();
const Inventory& inventory();
bool read_sector(uint32_t device_index, uint64_t lba, void* out512);
bool read_recovery_anchor_sector(uint32_t device_index,uint64_t lba,uint64_t recovery_first_lba,uint64_t recovery_last_lba,void* out512);
bool read_recovery_journal_sector(uint32_t device_index,uint64_t lba,uint64_t recovery_first_lba,uint64_t recovery_last_lba,void* out512);
bool write_recovery_journal_sector(uint32_t device_index,uint64_t lba,uint64_t recovery_first_lba,uint64_t recovery_last_lba,const void* in512);
bool flush_device(uint32_t device_index);

}
