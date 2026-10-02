#pragma once
#include <stdint.h>
namespace peregrinus::security::itco {
struct ProbeResult{bool supported_chipset;bool acpi_base_valid;bool registers_read;bool running;bool arm_live;uint16_t device_id;uint16_t acpi_base;uint16_t tco_base;uint16_t tco1_cnt;uint16_t tco2_sts;uint16_t timer_ticks;};
ProbeResult probe_readonly();
bool self_test();
const ProbeResult& result();
}
