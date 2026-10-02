#pragma once
#include <stdint.h>
struct limine_rsdp_response;
struct limine_hhdm_response;
namespace peregrinus::acpi {
struct McfgInfo { uint64_t ecam_base; uint16_t segment; uint8_t start_bus; uint8_t end_bus; };
struct MadtInfo { uint64_t lapic_address; uint32_t local_apics; uint32_t io_apics; uint32_t interrupt_overrides; bool pcat_compat; };
void init(limine_rsdp_response* rsp, limine_hhdm_response* hhdm);
bool rsdp_present();
bool mcfg_present();
bool madt_present();
uint64_t hhdm_offset();
const McfgInfo& mcfg();
const MadtInfo& madt();
}
