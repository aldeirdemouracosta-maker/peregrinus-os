#include "storage.hpp"
#include "ahci.hpp"
#include "../config/features.hpp"
#include "../console/serial.hpp"
#include "../console/format.hpp"

namespace peregrinus::storage {
namespace { Inventory g{}; alignas(16) uint8_t identify_sector[512]; static void clear(void* p,uint32_t n){auto* b=(uint8_t*)p;for(uint32_t i=0;i<n;++i)b[i]=0;} }

void init(){
    g={};
    const auto& a=ahci::result();
    for(uint8_t p=0; p<32 && g.count<8; ++p){
        const auto& ap=a.ports[p];
        if(!ap.implemented || !ap.device_present || !ap.interface_active || ap.kind!=ahci::PortKind::sata) continue;
        auto& d=g.devices[g.count++];
        d.kind=DeviceKind::ahci_sata; d.controller_index=0; d.port_index=p; d.readonly=true; d.present=true;
        if(features::ahci_dma_read_live){
            clear(identify_sector,sizeof(identify_sector));
            if(ahci::identify_device_guarded(p,identify_sector)){
                d.info=identify::parse(identify_sector,sizeof(identify_sector)); d.identified=d.info.valid;
            }
        }
    }
    char n[24]; format::dec64(g.count,n); serial::write("Storage devices (read-only abstraction): "); serial::writeln(n);
    for(uint32_t i=0;i<g.count;++i){
        const auto& d=g.devices[i]; serial::write("  SATA port "); format::dec64(d.port_index,n); serial::write(n); serial::write(": ");
        if(!d.identified){serial::writeln(features::ahci_dma_read_live?"IDENTIFY rejected":"IDENTIFY deferred (safe build)");continue;}
        serial::write(d.info.model[0]?d.info.model:"(model unavailable)"); serial::write(" sectors=");format::dec64(d.info.sector_count,n);serial::write(n);
        serial::write(" logical=");format::dec64(d.info.logical_sector_bytes,n);serial::write(n);serial::writeln("B");
    }
}

const Inventory& inventory(){ return g; }

bool read_sector(uint32_t device_index,uint64_t lba,void* out512){
    if(device_index>=g.count || !out512) return false;
    const auto& d=g.devices[device_index];
    if(!d.present || d.kind!=DeviceKind::ahci_sata) return false;
    if(features::ahci_dma_read_live && !d.identified) return false;
    if(d.identified && (d.info.logical_sector_bytes!=512u || lba>d.info.last_lba)) return false;
    return ahci::read_metadata_sector_guarded(d.port_index,lba,d.info.last_lba,out512);
}
bool read_recovery_anchor_sector(uint32_t device_index,uint64_t lba,uint64_t recovery_first_lba,uint64_t recovery_last_lba,void* out512){
    if(device_index>=g.count || !out512) return false;
    const auto& d=g.devices[device_index];
    if(!d.present || d.kind!=DeviceKind::ahci_sata || !d.identified || d.info.logical_sector_bytes!=512u) return false;
    if(lba>d.info.last_lba) return false;
    return ahci::read_recovery_anchor_sector_guarded(d.port_index,lba,d.info.last_lba,recovery_first_lba,recovery_last_lba,out512);
}

bool read_recovery_journal_sector(uint32_t device_index,uint64_t lba,uint64_t recovery_first_lba,uint64_t recovery_last_lba,void* out512){
    if(device_index>=g.count||!out512)return false;
    const auto& d=g.devices[device_index];
    if(!d.present||d.kind!=DeviceKind::ahci_sata||!d.identified||d.info.logical_sector_bytes!=512u)return false;
    if(lba>d.info.last_lba)return false;
    return ahci::read_recovery_journal_sector_guarded(d.port_index,lba,d.info.last_lba,recovery_first_lba,recovery_last_lba,out512);
}
bool write_recovery_journal_sector(uint32_t device_index,uint64_t lba,uint64_t recovery_first_lba,uint64_t recovery_last_lba,const void* in512){
    if(device_index>=g.count||!in512||!features::recovery_journal_write_live)return false;
    const auto& d=g.devices[device_index];
    if(!d.present||d.kind!=DeviceKind::ahci_sata||!d.identified||d.info.logical_sector_bytes!=512u)return false;
    if(lba>d.info.last_lba)return false;
    return ahci::write_recovery_journal_sector_guarded(d.port_index,lba,d.info.last_lba,recovery_first_lba,recovery_last_lba,in512);
}
bool flush_device(uint32_t device_index){
    if(device_index>=g.count||!features::recovery_journal_write_live)return false;
    const auto& d=g.devices[device_index];
    if(!d.present||d.kind!=DeviceKind::ahci_sata||!d.identified)return false;
    return ahci::flush_cache_guarded(d.port_index);
}
}
