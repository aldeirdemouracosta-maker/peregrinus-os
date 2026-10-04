#pragma once
#include <stdint.h>
namespace peregrinus::pci {
struct BarInfo { uint64_t address; bool io; bool is64; };
struct Device {
    uint8_t bus,slot,function;
    uint8_t class_code,subclass,prog_if,header_type;
    uint16_t vendor,device;
};
struct Summary { uint32_t functions,amd_devices,nvidia_devices,display_devices,storage_devices,recorded_devices; };
Summary enumerate_readonly();
const Summary& summary();
const Device* devices();
uint32_t device_count();
uint32_t config_read32(const Device& d,uint16_t off);
uint16_t config_read16(const Device& d,uint16_t off);
BarInfo bar_info(const Device& d,uint8_t index);
uint16_t command_register(const Device& d);
bool memory_space_enabled(const Device& d);
bool enable_memory_busmaster(const Device& d);
// Memory decoding only (no DMA). Bus mastering is granted separately, last, once the device
// has been reset and its DMA rings point at memory the kernel owns.
bool enable_memory_space(const Device& d);
bool enable_bus_master(const Device& d);
void disable_bus_master(const Device& d);
}
