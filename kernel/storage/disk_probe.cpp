#include "disk_probe.hpp"
#include "storage.hpp"
#include "gpt.hpp"
#include "../config/features.hpp"
#include "../console/serial.hpp"
#include "../console/format.hpp"

namespace peregrinus::disk_probe {
namespace {
alignas(16) static uint8_t g_primary_sector[512];
alignas(16) static uint8_t g_backup_sector[512];
alignas(16) static uint8_t g_primary_entries[16384];
alignas(16) static uint8_t g_backup_entries[16384];
static gpt::TableInfo g_table{};
static bool g_table_valid=false;
static void clear_bytes(uint8_t* p,uint32_t n){ for(uint32_t i=0;i<n;++i)p[i]=0; }
static bool bytes_equal(const uint8_t* a,const uint8_t* b,uint32_t n){ for(uint32_t i=0;i<n;++i) if(a[i]!=b[i]) return false; return true; }
static bool read_entries(uint32_t dev,const gpt::HeaderInfo& h,uint8_t* out,uint32_t capacity,uint32_t& out_bytes){
    out_bytes=0;
    if(h.entry_count==0 || h.entry_size<128 || h.entry_size>4096) return false;
    const uint64_t total64=uint64_t(h.entry_count)*uint64_t(h.entry_size);
    if(h.entry_count && total64/h.entry_count!=h.entry_size) return false;
    if(total64==0 || total64>capacity) return false;
    const uint32_t total=(uint32_t)total64;
    const uint32_t sectors=(total+511u)/512u;
    clear_bytes(out,capacity);
    for(uint32_t i=0;i<sectors;++i){ if(!storage::read_sector(dev,h.entries_lba+i,out+i*512u)) return false; }
    out_bytes=total; return true;
}
static void report_table(Result& r,const gpt::TableInfo& t){
    r.used_partitions=t.used_entries; char n[24]; format::dec64(t.used_entries,n); serial::write("GPT selected used partitions: "); serial::writeln(n);
    for(uint32_t i=0;i<t.used_entries;++i){
        const auto& p=t.partitions[i]; serial::write("  "); serial::write(p.name[0]?p.name:"(unnamed)"); serial::write(" role="); serial::writeln(gpt::role_name(p.role));
        if(p.role==gpt::PartitionRole::system) r.has_system=true;
        if(p.role==gpt::PartitionRole::recovery) r.has_recovery=true;
        if(p.role==gpt::PartitionRole::data) r.has_data=true;
    }
}
}

Result run_qemu_gpt_probe(){
    Result r{}; g_table={}; g_table_valid=false;
    // Read-only GPT probe uses the same gate as the recovery anchor/journal
    // readers: disposable QEMU disks or the recovery-commit-live profile. Gating
    // it on disposable disks alone left recovery-live without boot health, so
    // the Guard always halted before the boot-success commit.
    if(!features::ahci_dma_read_live || !features::recovery_metadata_live){ serial::writeln("Noe 1.0 redundant GPT probe: SKIPPED (safe default build)"); return r; }
    r.attempted=true;
    if(storage::inventory().count==0){ serial::writeln("Noe 1.0 redundant GPT probe: no storage device"); return r; }
    const auto& dev=storage::inventory().devices[0]; r.device_identified=dev.identified; r.logical_sector_bytes=dev.info.logical_sector_bytes; r.device_last_lba=dev.info.last_lba;
    if(!dev.identified){ serial::writeln("Noe 1.0 redundant GPT probe: IDENTIFY DEVICE unavailable/invalid"); return r; }
    if(dev.info.logical_sector_bytes!=512u || dev.info.last_lba<67u){ serial::writeln("Noe 1.0 redundant GPT probe: unsupported sector geometry"); return r; }

    clear_bytes(g_primary_sector,sizeof(g_primary_sector)); clear_bytes(g_backup_sector,sizeof(g_backup_sector));
    if(storage::read_sector(0,1,g_primary_sector)) r.primary_header_read=true;
    if(storage::read_sector(0,dev.info.last_lba,g_backup_sector)) r.backup_header_read=true;

    const auto ph=gpt::parse_header(g_primary_sector,sizeof(g_primary_sector)); const auto bh=gpt::parse_header(g_backup_sector,sizeof(g_backup_sector));
    const gpt::TableInfo empty{};
    const auto ph_pre=gpt::validate_copy(ph,empty,gpt::CopyKind::primary,dev.info.last_lba,512);
    const auto bh_pre=gpt::validate_copy(bh,empty,gpt::CopyKind::backup,dev.info.last_lba,512);

    uint32_t pbytes=0,bbytes=0; gpt::TableInfo pt{},bt{};
    if(ph_pre.header_ok&&ph_pre.layout_ok&&read_entries(0,ph,g_primary_entries,sizeof(g_primary_entries),pbytes)) pt=gpt::parse_entries(ph,g_primary_entries,pbytes);
    if(bh_pre.header_ok&&bh_pre.layout_ok&&read_entries(0,bh,g_backup_entries,sizeof(g_backup_entries),bbytes)) bt=gpt::parse_entries(bh,g_backup_entries,bbytes);

    const auto pv=gpt::validate_copy(ph,pt,gpt::CopyKind::primary,dev.info.last_lba,512); const auto bv=gpt::validate_copy(bh,bt,gpt::CopyKind::backup,dev.info.last_lba,512);
    const bool arrays_equal=pbytes!=0 && pbytes==bbytes && bytes_equal(g_primary_entries,g_backup_entries,pbytes);
    const auto rr=gpt::assess_redundancy(ph,pt,pv,bh,bt,bv,arrays_equal);
    r.primary_valid=rr.primary_valid; r.backup_valid=rr.backup_valid; r.headers_coherent=rr.headers_coherent; r.tables_match=rr.tables_match; r.split_brain=rr.split_brain; r.degraded=rr.degraded; r.selected=rr.selected;

    serial::write("GPT primary: "); serial::writeln(r.primary_valid?"VALID":"INVALID");
    serial::write("GPT backup:  "); serial::writeln(r.backup_valid?"VALID":"INVALID");
    serial::write("GPT selection: "); serial::writeln(gpt::selection_name(r.selected));
    if(r.split_brain) serial::writeln("GPT REDUNDANCY: SPLIT-BRAIN; automatic selection blocked");
    else if(r.degraded) serial::writeln("GPT REDUNDANCY: DEGRADED; valid copy selected read-only");
    else if(r.selected!=gpt::Selection::none) serial::writeln("GPT REDUNDANCY: HEALTHY; primary and backup agree");

    const gpt::TableInfo* chosen=nullptr;
    if(r.selected==gpt::Selection::primary) chosen=&pt; else if(r.selected==gpt::Selection::backup) chosen=&bt;
    if(chosen){ g_table=*chosen; g_table_valid=true; r.disk_guid=(r.selected==gpt::Selection::primary?ph.disk_guid:bh.disk_guid); report_table(r,*chosen); }
    return r;
}

const gpt::TableInfo& last_table(){ return g_table; }
bool has_valid_table(){ return g_table_valid; }
}
