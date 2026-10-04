#include "ahci.hpp"
#include "../pci/pci.hpp"
#include "../mm/memory.hpp"
#include "../mm/mmio.hpp"
#include "../console/serial.hpp"
#include "../console/format.hpp"
#include "../config/features.hpp"
#include "../runtime/spin.hpp"

namespace peregrinus::ahci {
namespace {
struct __attribute__((packed)) HbaPort { uint32_t clb,clbu,fb,fbu,is,ie,cmd,rsv0,tfd,sig,ssts,sctl,serr,sact,ci,sntf,fbs,devslp; uint32_t rsv1[10]; uint32_t vendor[4]; };
struct __attribute__((packed)) HbaMem { uint32_t cap,ghc,is,pi,vs,ccc_ctl,ccc_pts,em_loc,em_ctl,cap2,bohc; uint8_t rsv[0xA0-0x2C]; uint8_t vendor[0x100-0xA0]; HbaPort ports[32]; };
struct __attribute__((packed)) HbaPrdtEntry { uint32_t dba,dbau,reserved,dbc_i; };
struct __attribute__((packed)) HbaCmdHeader { uint16_t flags,prdtl; uint32_t prdbc,ctba,ctbau,reserved[4]; };
struct __attribute__((packed)) HbaCmdTable { uint8_t cfis[64],acmd[16],reserved[48]; HbaPrdtEntry prdt[1]; };
struct __attribute__((packed)) FisRegH2d { uint8_t fis_type,pmport_c,command,featurel,lba0,lba1,lba2,device,lba3,lba4,lba5,featureh; uint16_t count; uint8_t icc,control,reserved[4]; };
static_assert(sizeof(HbaCmdHeader)==32); static_assert(sizeof(HbaCmdTable)==144); static_assert(sizeof(FisRegH2d)==20);
struct DmaWorkspace { bool ready; uint64_t clb_phys,fis_phys,tbl_phys,data_phys; HbaCmdHeader* cl; uint8_t* rfis; HbaCmdTable* table; uint8_t* data; };

static ProbeResult g{}; static DmaWorkspace ws{}; static volatile HbaMem* g_hba=nullptr; static bool g_poisoned=false;
static constexpr uint32_t SATA_SIG_ATA=0x00000101u,SATA_SIG_ATAPI=0xEB140101u,SATA_SIG_SEMB=0xC33C0101u,SATA_SIG_PM=0x96690101u;
static constexpr uint8_t FIS_TYPE_REG_H2D=0x27,ATA_CMD_READ_DMA_EXT=0x25,ATA_CMD_WRITE_DMA_EXT=0x35,ATA_CMD_FLUSH_EXT=0xEA,ATA_CMD_IDENTIFY_DEVICE=0xEC;
static constexpr uint32_t HBA_PxCMD_ST=1u<<0,HBA_PxCMD_FRE=1u<<4,HBA_PxCMD_FR=1u<<14,HBA_PxCMD_CR=1u<<15,HBA_PxIS_TFES=1u<<30;
static constexpr uint32_t ATA_DEV_BUSY=0x80,ATA_DEV_DRQ=0x08,GHC_AE=1u<<31,CAP2_BOH=1u<<0,BOHC_BOS=1u<<0,BOHC_OOS=1u<<1,BOHC_BB=1u<<4;

static PortKind kind_for(uint32_t sig){if(sig==SATA_SIG_ATA)return PortKind::sata;if(sig==SATA_SIG_ATAPI)return PortKind::satapi;if(sig==SATA_SIG_SEMB)return PortKind::semb;if(sig==SATA_SIG_PM)return PortKind::port_multiplier;return sig?PortKind::unknown:PortKind::none;}
static const char* kind_name(PortKind k){switch(k){case PortKind::sata:return "SATA";case PortKind::satapi:return "SATAPI";case PortKind::semb:return "SEMB";case PortKind::port_multiplier:return "PM";case PortKind::unknown:return "UNKNOWN";default:return "NONE";}}
static void zero(void* p,uint32_t n){auto* b=(uint8_t*)p;for(uint32_t i=0;i<n;++i)b[i]=0;}
static void copy512(void* d0,const void* s0){auto* d=(uint8_t*)d0;auto* s=(const uint8_t*)s0;for(uint32_t i=0;i<512;++i)d[i]=s[i];}
static const pci::Device* controller_device(){const auto* ds=pci::devices();for(uint32_t i=0;i<pci::device_count();++i){const auto& d=ds[i];if(d.class_code==0x01&&d.subclass==0x06&&d.prog_if==0x01)return &d;}return nullptr;}
static bool wait_cmd_clear(volatile HbaPort& p,uint32_t mask,uint32_t spins){return spin::until([&]{return (p.cmd&mask)==0;},spins);}
static bool stop_port(volatile HbaPort& p){p.cmd&=~HBA_PxCMD_ST;p.cmd&=~HBA_PxCMD_FRE;return wait_cmd_clear(p,HBA_PxCMD_FR|HBA_PxCMD_CR,1000000);}
static bool start_port(volatile HbaPort& p){if(!wait_cmd_clear(p,HBA_PxCMD_CR,1000000))return false;p.cmd|=HBA_PxCMD_FRE;p.cmd|=HBA_PxCMD_ST;return true;}
static int find_slot(volatile HbaPort& p){uint32_t slots=p.sact|p.ci;uint32_t n=g.command_slots?g.command_slots:1;if(n>32)n=32;for(uint32_t i=0;i<n;++i)if((slots&(1u<<i))==0)return int(i);return -1;}
static bool init_workspace(){
    if(ws.ready) return true;
    ws={};
    ws.clb_phys=reinterpret_cast<uint64_t>(memory::alloc_dma32_page()); ws.fis_phys=reinterpret_cast<uint64_t>(memory::alloc_dma32_page());
    ws.tbl_phys=reinterpret_cast<uint64_t>(memory::alloc_dma32_page()); ws.data_phys=reinterpret_cast<uint64_t>(memory::alloc_dma32_page());
    if(!ws.clb_phys||!ws.fis_phys||!ws.tbl_phys||!ws.data_phys)return false;
    ws.cl=(HbaCmdHeader*)memory::phys_to_hhdm(ws.clb_phys); ws.rfis=(uint8_t*)memory::phys_to_hhdm(ws.fis_phys);
    ws.table=(HbaCmdTable*)memory::phys_to_hhdm(ws.tbl_phys); ws.data=(uint8_t*)memory::phys_to_hhdm(ws.data_phys);
    if(!ws.cl||!ws.rfis||!ws.table||!ws.data)return false;
    zero(ws.cl,4096);zero(ws.rfis,4096);zero(ws.table,4096);zero(ws.data,4096);ws.ready=true;g.dma_workspace_ready=true;return true;
}
static bool take_ownership(){
    if(!g_hba) return false;
    if(!g.bios_os_handoff_supported) return true;
    uint32_t v=g_hba->bohc;g.bios_owned=(v&BOHC_BOS)!=0;g.os_owned=(v&BOHC_OOS)!=0;g.bios_busy=(v&BOHC_BB)!=0;
    if(!g.bios_owned&&!g.bios_busy){g_hba->bohc=v|BOHC_OOS;g.os_owned=true;return true;}
    g_hba->bohc=v|BOHC_OOS;
    if(spin::until([&]{v=g_hba->bohc;return (v&(BOHC_BOS|BOHC_BB))==0;},5000000)){g.bios_owned=false;g.bios_busy=false;g.os_owned=(v&BOHC_OOS)!=0;return g.os_owned;}
    serial::writeln("AHCI: BIOS/OS handoff timeout");return false;
}
static bool prepare_live(uint8_t port_index,volatile HbaPort*& port,int& slot){
    if((!features::ahci_dma_read_live && !features::recovery_journal_write_live) || port_index>=32 || !g_hba) return false;
    if(!g.ports[port_index].device_present||!g.ports[port_index].interface_active||g.ports[port_index].kind!=PortKind::sata)return false;
    const pci::Device* ctl=controller_device();if(!ctl||!pci::enable_memory_busmaster(*ctl))return false;
    if(!take_ownership()||!init_workspace())return false;
    g_hba->ghc|=GHC_AE;port=&g_hba->ports[port_index];if(!stop_port(*port))return false;
    zero(ws.cl,4096);zero(ws.rfis,4096);zero(ws.table,4096);zero(ws.data,4096);
    port->clb=(uint32_t)ws.clb_phys;port->clbu=(uint32_t)(ws.clb_phys>>32);port->fb=(uint32_t)ws.fis_phys;port->fbu=(uint32_t)(ws.fis_phys>>32);
    if(!start_port(*port)) return false;
    slot=find_slot(*port);
    return slot>=0;
}
enum class Transfer : uint8_t { none, data_in, data_out };
// One command, one 512-byte sector at most. Any timeout or task-file error after the doorbell
// leaves the command state unknown (the HBA may still DMA into the workspace), so the
// controller is poisoned: every later command is refused (fail-closed) until reboot.
static bool issue(uint8_t port_index,uint8_t command,uint64_t lba,bool lba48,Transfer t,void* buf512){
    if(g_poisoned)return false;
    if(t!=Transfer::none&&!buf512)return false;
    if(t==Transfer::data_out&&!features::recovery_journal_write_live)return false;
    volatile HbaPort* port=nullptr;int slot=-1;
    if(!prepare_live(port_index,port,slot))return false;
    if(t==Transfer::data_out)copy512(ws.data,buf512);
    HbaCmdHeader& hdr=ws.cl[slot];hdr.flags=(uint16_t)(5u|(t==Transfer::data_out?(1u<<6):0u));hdr.prdtl=t==Transfer::none?0:1;hdr.prdbc=0;hdr.ctba=(uint32_t)ws.tbl_phys;hdr.ctbau=(uint32_t)(ws.tbl_phys>>32);
    HbaCmdTable* ct=ws.table;zero(ct,sizeof(HbaCmdTable));
    if(t!=Transfer::none){ct->prdt[0].dba=(uint32_t)ws.data_phys;ct->prdt[0].dbau=(uint32_t)(ws.data_phys>>32);ct->prdt[0].dbc_i=(512u-1u);}
    auto* fis=(FisRegH2d*)ct->cfis;fis->fis_type=FIS_TYPE_REG_H2D;fis->pmport_c=1u<<7;fis->command=command;
    if(lba48){fis->device=1u<<6;fis->lba0=(uint8_t)lba;fis->lba1=(uint8_t)(lba>>8);fis->lba2=(uint8_t)(lba>>16);fis->lba3=(uint8_t)(lba>>24);fis->lba4=(uint8_t)(lba>>32);fis->lba5=(uint8_t)(lba>>40);fis->count=1;}
    // Before the doorbell nothing is in flight: a busy device is a plain refusal.
    if(!spin::until([&]{return (port->tfd&(ATA_DEV_BUSY|ATA_DEV_DRQ))==0;},1000000))return false;
    port->is=port->is;asm volatile("mfence":::"memory");port->ci|=(1u<<slot); // W1C: clear only asserted status bits
    const uint32_t bit=1u<<slot;
    const bool completed=spin::until([&]{return (port->ci&bit)==0||(port->is&HBA_PxIS_TFES)!=0;},5000000);
    if(!completed||(port->is&HBA_PxIS_TFES)!=0||(port->ci&bit)!=0){
        g_poisoned=true;g.poisoned=true;(void)stop_port(*port);
        serial::writeln("AHCI: command timeout/error; controller POISONED (fail-closed)");
        return false;
    }
    asm volatile("mfence":::"memory");
    if(t==Transfer::data_in)copy512(buf512,ws.data);
    return true;
}
static bool issue_512_data_in(uint8_t port_index,uint8_t command,uint64_t lba,bool lba48,void* out512){return issue(port_index,command,lba,lba48,Transfer::data_in,out512);}
static bool issue_512_data_out(uint8_t port_index,uint8_t command,uint64_t lba,const void* in512){return issue(port_index,command,lba,true,Transfer::data_out,const_cast<void*>(in512));}
static bool issue_no_data(uint8_t port_index,uint8_t command){if(!features::recovery_journal_write_live)return false;return issue(port_index,command,0,false,Transfer::none,nullptr);}
}

ProbeResult inspect_readonly(){
    g={};ws={};g_hba=nullptr;g_poisoned=false;g.readonly_safe=true;const auto* ds=pci::devices();const pci::Device* ctl=nullptr;
    for(uint32_t i=0;i<pci::device_count();++i){const auto& d=ds[i];if(d.class_code==0x01&&d.subclass==0x06&&d.prog_if==0x01){++g.controllers;if(!g.abar){g.abar=pci::bar_info(d,5).address;ctl=&d;}}}
    char n[24],h[19];format::dec64(g.controllers,n);serial::write("AHCI controllers: ");serial::writeln(n);
    if(!g.abar||!ctl){serial::writeln("AHCI: ABAR unavailable");return g;}
    if(!pci::memory_space_enabled(*ctl)){
        if(features::ahci_dma_read_live){if(!pci::enable_memory_busmaster(*ctl)){serial::writeln("AHCI: QEMU test could not enable PCI Memory Space");return g;}}
        else {serial::writeln("AHCI: PCI Memory Space disabled; safe inspection skipped");return g;}
    }
    format::hex64(g.abar,h);serial::write("AHCI ABAR: ");serial::writeln(h);
    g_hba=reinterpret_cast<volatile HbaMem*>(mmio::map(g.abar,sizeof(HbaMem)));
    if(!g_hba){serial::writeln("AHCI: dedicated MMIO mapping failed");return g;}
    g.mmio_mapped=true;g.host_capabilities=g_hba->cap;g.host_capabilities2=g_hba->cap2;g.version=g_hba->vs;g.command_slots=((g.host_capabilities>>8)&0x1Fu)+1;
    g.bios_os_handoff_supported=(g.host_capabilities2&CAP2_BOH)!=0;uint32_t bohc=g_hba->bohc;g.bios_owned=(bohc&BOHC_BOS)!=0;g.os_owned=(bohc&BOHC_OOS)!=0;g.bios_busy=(bohc&BOHC_BB)!=0;
    const uint32_t pi=g_hba->pi;g.mmio_inspected=true;
    for(uint8_t p=0;p<32;++p){if((pi&(1u<<p))==0)continue;++g.implemented_ports;const volatile HbaPort& hp=g_hba->ports[p];PortInfo info{};info.index=p;info.implemented=true;info.signature=hp.sig;info.ssts=hp.ssts;
        const uint8_t det=uint8_t(hp.ssts&0x0Fu),ipm=uint8_t((hp.ssts>>8)&0x0Fu);info.device_present=(det==3);info.interface_active=(ipm==1);info.kind=kind_for(hp.sig);info.command_list_base=uint64_t(hp.clb)|(uint64_t(hp.clbu)<<32);info.fis_base=uint64_t(hp.fb)|(uint64_t(hp.fbu)<<32);g.ports[p]=info;
        if(info.device_present&&info.interface_active){++g.present_ports;if(info.kind==PortKind::sata)++g.sata_ports;if(info.kind==PortKind::satapi)++g.satapi_ports;}
        serial::write("AHCI port ");format::dec64(p,n);serial::write(n);serial::write(": ");serial::write(kind_name(info.kind));serial::write(info.device_present?" present":" absent");serial::writeln(info.interface_active?" active":" inactive");}
    format::dec64(g.command_slots,n);serial::write("AHCI command slots: ");serial::writeln(n);
    serial::writeln("AHCI policy: dedicated UC MMIO mapping; device writes remain gated");return g;
}

DmaSelfTestResult dma_self_test(){DmaSelfTestResult r{};r.command_header_size=sizeof(HbaCmdHeader);r.command_table_size=sizeof(HbaCmdTable);r.layout_ok=(sizeof(HbaCmdHeader)==32&&sizeof(HbaCmdTable)==144&&sizeof(FisRegH2d)==20);r.live_enabled=features::ahci_dma_read_live;r.safety_gate_ok=!features::ahci_dma_read_live;r.workspace_ready=!features::ahci_dma_read_live||init_workspace();serial::writeln(r.layout_ok?"AHCI DMA layout self-test: PASS":"AHCI DMA layout self-test: FAIL");serial::writeln(features::ahci_dma_read_live?"AHCI data-in live gate: ENABLED":"AHCI data-in live gate: DISABLED (safe default)");serial::writeln(r.workspace_ready?"AHCI reusable DMA workspace: READY":"AHCI reusable DMA workspace: NOT READY");return r;}

bool identify_device_guarded(uint8_t port_index,void* out512){ return issue_512_data_in(port_index,ATA_CMD_IDENTIFY_DEVICE,0,false,out512); }
bool read_metadata_sector_guarded(uint8_t port_index,uint64_t lba,uint64_t device_last_lba,void* out512){
    if(!features::ahci_dma_read_live || features::ahci_max_sectors_per_command<1 || !out512) return false;
    const bool head=lba<=features::gpt_primary_metadata_max_lba;
    const bool tail=device_last_lba>=features::gpt_backup_metadata_sectors && lba<=device_last_lba &&
                    lba>=(device_last_lba-(features::gpt_backup_metadata_sectors-1u));
    if(!head && !tail) return false;
    return issue_512_data_in(port_index,ATA_CMD_READ_DMA_EXT,lba,true,out512);
}
bool read_recovery_anchor_sector_guarded(uint8_t port_index,uint64_t lba,uint64_t device_last_lba,uint64_t recovery_first_lba,uint64_t recovery_last_lba,void* out512){
    if(!features::recovery_anchor_read_live || !features::recovery_metadata_live || !out512) return false;
    if(recovery_first_lba>recovery_last_lba || recovery_last_lba>device_last_lba) return false;
    const uint64_t backup_floor=device_last_lba>=features::gpt_backup_metadata_sectors ? device_last_lba-(features::gpt_backup_metadata_sectors-1u) : 0;
    if(recovery_first_lba<=features::gpt_primary_metadata_max_lba || recovery_last_lba>=backup_floor) return false;
    if(lba!=recovery_first_lba && lba!=recovery_last_lba) return false;
    return issue_512_data_in(port_index,ATA_CMD_READ_DMA_EXT,lba,true,out512);
}
bool read_recovery_journal_sector_guarded(uint8_t port_index,uint64_t lba,uint64_t device_last_lba,uint64_t recovery_first_lba,uint64_t recovery_last_lba,void* out512){
    if(!features::recovery_journal_read_live||!features::recovery_metadata_live||!out512)return false;
    if(recovery_first_lba+5u>recovery_last_lba||recovery_last_lba>device_last_lba)return false;
    const uint64_t backup_floor=device_last_lba>=features::gpt_backup_metadata_sectors?device_last_lba-(features::gpt_backup_metadata_sectors-1u):0;
    if(recovery_first_lba<=features::gpt_primary_metadata_max_lba||recovery_last_lba>=backup_floor)return false;
    if(lba!=recovery_first_lba+2u&&lba!=recovery_last_lba-2u)return false;
    return issue_512_data_in(port_index,ATA_CMD_READ_DMA_EXT,lba,true,out512);
}
bool write_recovery_journal_sector_guarded(uint8_t port_index,uint64_t lba,uint64_t device_last_lba,uint64_t recovery_first_lba,uint64_t recovery_last_lba,const void* in512){
    if(!features::recovery_journal_write_live||!in512)return false;
    if(recovery_first_lba+5u>recovery_last_lba||recovery_last_lba>device_last_lba)return false;
    const uint64_t backup_floor=device_last_lba>=features::gpt_backup_metadata_sectors?device_last_lba-(features::gpt_backup_metadata_sectors-1u):0;
    if(recovery_first_lba<=features::gpt_primary_metadata_max_lba||recovery_last_lba>=backup_floor)return false;
    if(lba!=recovery_first_lba+2u&&lba!=recovery_last_lba-2u)return false;
    return issue_512_data_out(port_index,ATA_CMD_WRITE_DMA_EXT,lba,in512);
}
bool flush_cache_guarded(uint8_t port_index){return issue_no_data(port_index,ATA_CMD_FLUSH_EXT);}

int first_active_sata_port(){for(int i=0;i<32;++i)if(g.ports[i].implemented&&g.ports[i].device_present&&g.ports[i].interface_active&&g.ports[i].kind==PortKind::sata)return i;return -1;}
const ProbeResult& result(){return g;}
}
