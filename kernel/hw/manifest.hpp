#pragma once
#include <stdint.h>
namespace peregrinus::hw {
struct Manifest {
    char cpu_vendor[13];
    uint64_t usable_memory_bytes;
    uint32_t pci_functions;
    uint32_t amd_devices;
    uint32_t nvidia_devices;
    uint32_t display_devices;
    uint32_t storage_devices;
    uint64_t mmio_mappings;
    uint32_t local_apics;
    uint32_t io_apics;
    bool framebuffer_ready;
    bool rsdp_present;
    bool mcfg_present;
    bool madt_present;
};
void init();
const Manifest& manifest();
void print();
}
