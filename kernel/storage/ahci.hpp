#pragma once
#include <stdint.h>
namespace peregrinus::ahci {
enum class PortKind : uint8_t { none, sata, satapi, semb, port_multiplier, unknown };
struct PortInfo { uint8_t index; bool implemented; bool device_present; bool interface_active; PortKind kind; uint32_t signature; uint32_t ssts; uint64_t command_list_base; uint64_t fis_base; };
struct ProbeResult {
    uint32_t controllers; uint64_t abar; bool readonly_safe; bool mmio_inspected; bool mmio_mapped;
    uint32_t implemented_ports,present_ports,sata_ports,satapi_ports;
    uint32_t host_capabilities,host_capabilities2,version,command_slots;
    bool bios_os_handoff_supported,bios_owned,os_owned,bios_busy,dma_workspace_ready;
    bool poisoned; // a command timed out or failed after the doorbell; all further commands refused
    PortInfo ports[32];
};
struct DmaSelfTestResult { bool layout_ok,safety_gate_ok,live_enabled,workspace_ready; uint32_t command_header_size,command_table_size; };
ProbeResult inspect_readonly();
const ProbeResult& result();
DmaSelfTestResult dma_self_test();
bool identify_device_guarded(uint8_t port_index,void* out512);
bool read_metadata_sector_guarded(uint8_t port_index,uint64_t lba,uint64_t device_last_lba,void* out512);
bool read_recovery_anchor_sector_guarded(uint8_t port_index,uint64_t lba,uint64_t device_last_lba,uint64_t recovery_first_lba,uint64_t recovery_last_lba,void* out512);
bool read_recovery_journal_sector_guarded(uint8_t port_index,uint64_t lba,uint64_t device_last_lba,uint64_t recovery_first_lba,uint64_t recovery_last_lba,void* out512);
bool write_recovery_journal_sector_guarded(uint8_t port_index,uint64_t lba,uint64_t device_last_lba,uint64_t recovery_first_lba,uint64_t recovery_last_lba,const void* in512);
bool flush_cache_guarded(uint8_t port_index);
int first_active_sata_port();
}
