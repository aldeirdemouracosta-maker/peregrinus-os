#include "recovery_probe.hpp"
#include "volume.hpp"
#include "storage.hpp"
#include "../config/features.hpp"
#include "../console/serial.hpp"
#include "../console/format.hpp"

namespace peregrinus::recovery_probe {
namespace {
alignas(16) static uint8_t first_sector[512];
alignas(16) static uint8_t last_sector[512];
static void clear(uint8_t* p,uint32_t n){for(uint32_t i=0;i<n;++i)p[i]=0;}
static bool matches(const recovery_anchor::AnchorInfo& a,const gpt::Guid& disk,const volume::Volume& sys,const volume::Volume& rec,const volume::Volume& data){
    return gpt::guid_equal(a.disk_guid,disk)&&gpt::guid_equal(a.recovery_guid,rec.unique_guid)&&gpt::guid_equal(a.system_guid,sys.unique_guid)&&gpt::guid_equal(a.data_guid,data.unique_guid)&&
           a.system_first_lba==sys.first_lba&&a.system_last_lba==sys.last_lba&&a.recovery_first_lba==rec.first_lba&&a.recovery_last_lba==rec.last_lba&&a.data_first_lba==data.first_lba&&a.data_last_lba==data.last_lba;
}
}

Result run(const gpt::Guid& disk_guid){
    Result r{};
    if(!features::recovery_anchor_read_live){serial::writeln("Recovery anchor probe: SKIPPED (safe default build)");return r;}
    r.attempted=true;
    const auto* sys=volume::by_role(volume::Role::system);const auto* rec=volume::by_role(volume::Role::recovery);const auto* data=volume::by_role(volume::Role::data);
    if(!sys||!rec||!data){serial::writeln("Recovery anchor probe: protected volume set incomplete");return r;}
    r.recovery_volume_present=true;clear(first_sector,sizeof(first_sector));clear(last_sector,sizeof(last_sector));
    r.first_read=storage::read_recovery_anchor_sector(rec->device_index,rec->first_lba,rec->first_lba,rec->last_lba,first_sector);
    r.last_read=storage::read_recovery_anchor_sector(rec->device_index,rec->last_lba,rec->first_lba,rec->last_lba,last_sector);
    const auto first=recovery_anchor::parse(first_sector,sizeof(first_sector));const auto last=recovery_anchor::parse(last_sector,sizeof(last_sector));
    r.first_layout_ok=r.first_read&&matches(first,disk_guid,*sys,*rec,*data);r.last_layout_ok=r.last_read&&matches(last,disk_guid,*sys,*rec,*data);
    r.assessment=recovery_anchor::assess(first,r.first_layout_ok,last,r.last_layout_ok);
    serial::write("Recovery anchor A: ");serial::writeln(r.assessment.first_valid?"VALID":"INVALID");
    serial::write("Recovery anchor B: ");serial::writeln(r.assessment.last_valid?"VALID":"INVALID");
    serial::write("Recovery selection: ");serial::writeln(recovery_anchor::copy_name(r.assessment.selected));
    if(r.assessment.split_brain)serial::writeln("RECOVERY: SPLIT-BRAIN; automatic last-known-good selection blocked");
    else if(r.assessment.last_known_good){char n[24];format::dec64(r.assessment.selected_generation,n);serial::write("RECOVERY: LAST-KNOWN-GOOD generation ");serial::writeln(n);}
    else serial::writeln("RECOVERY: no trusted structural last-known-good anchor");
    return r;
}

}
