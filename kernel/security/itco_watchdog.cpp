#include "itco_watchdog.hpp"
#include "../pci/pci.hpp"
#include "../config/features.hpp"
#include "../console/serial.hpp"
#include "../console/format.hpp"
#include <peregrinus/io.hpp>
namespace peregrinus::security::itco {
namespace {ProbeResult g{};constexpr uint16_t INTEL=0x8086,PATSBURG_LPC=0x1d41;constexpr uint16_t ACPIBASE_OFF=0x40,TCO_OFF=0x60,TCO1_CNT_OFF=0x08,TCO2_STS_OFF=0x06,TCO_V2_TMR_OFF=0x12;constexpr uint16_t TCO_HALT=1u<<11;}
ProbeResult probe_readonly(){g={};g.arm_live=features::itco_watchdog_arm_live;const auto* ds=pci::devices();for(uint32_t i=0;i<pci::device_count();++i){const auto& d=ds[i];if(d.vendor!=INTEL||d.device!=PATSBURG_LPC)continue;g.supported_chipset=true;g.device_id=d.device;const uint32_t raw=pci::config_read32(d,ACPIBASE_OFF);const uint16_t base=(uint16_t)(raw&0xff80u);g.acpi_base=base;if(!base)break;g.acpi_base_valid=true;g.tco_base=(uint16_t)(base+TCO_OFF);g.tco1_cnt=io::in16((uint16_t)(g.tco_base+TCO1_CNT_OFF));g.tco2_sts=io::in16((uint16_t)(g.tco_base+TCO2_STS_OFF));g.timer_ticks=(uint16_t)(io::in16((uint16_t)(g.tco_base+TCO_V2_TMR_OFF))&0x03ffu);g.running=(g.tco1_cnt&TCO_HALT)==0;g.registers_read=true;break;}
    serial::writeln(g.supported_chipset?"iTCO: Intel Patsburg LPC detected":"iTCO: supported Patsburg LPC not detected");if(g.registers_read){char h[19];format::hex64(g.tco_base,h);serial::write("iTCO TCOBASE: ");serial::writeln(h);serial::writeln(g.running?"iTCO timer: already running":"iTCO timer: halted");}serial::writeln(g.arm_live?"iTCO ARM gate: ENABLED":"iTCO ARM gate: DISABLED (probe-only)");return g;}
bool self_test(){const uint16_t acpi=0x400;const uint16_t tco=(uint16_t)(acpi+TCO_OFF);const uint16_t halted=TCO_HALT;const uint16_t running=0;return tco==0x460&&((halted&TCO_HALT)!=0)&&((running&TCO_HALT)==0)&&!features::itco_watchdog_arm_live;}
const ProbeResult& result(){return g;}
}
