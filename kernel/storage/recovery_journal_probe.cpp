#include "recovery_journal_probe.hpp"
#include "storage.hpp"
#include "volume.hpp"
#include "../config/features.hpp"
#include "../console/serial.hpp"

namespace peregrinus::recovery_journal_probe {
namespace {
alignas(16) uint8_t a_sector[512];
alignas(16) uint8_t b_sector[512];
alignas(16) uint8_t verify_sector[512];

static void clear(void* p,uint32_t n){
    auto* b=(uint8_t*)p;
    for(uint32_t i=0;i<n;++i)b[i]=0;
}
static void encode(uint8_t out[512],const peregrinus_recovery_journal& j){
    clear(out,512);
    const auto* s=(const uint8_t*)&j;
    for(uint32_t i=0;i<sizeof(j);++i)out[i]=s[i];
}
static bool record_equal(const peregrinus_recovery_journal& a,const peregrinus_recovery_journal& b){
    const auto* x=(const uint8_t*)&a;
    const auto* y=(const uint8_t*)&b;
    for(uint32_t i=0;i<sizeof(a);++i)if(x[i]!=y[i])return false;
    return true;
}
struct Loaded {
    const volume::Volume* rec;
    uint64_t a_lba;
    uint64_t b_lba;
    recovery_journal::Assessment assessment;
    bool a_read;
    bool b_read;
};
static Loaded load(const gpt::Guid& disk_guid){
    Loaded x{};
    x.rec=volume::by_role(volume::Role::recovery);
    if(!x.rec||x.rec->last_lba<=x.rec->first_lba+5u)return x;
    x.a_lba=x.rec->first_lba+2u;
    x.b_lba=x.rec->last_lba-2u;
    clear(a_sector,512);clear(b_sector,512);
    x.a_read=storage::read_recovery_journal_sector(x.rec->device_index,x.a_lba,x.rec->first_lba,x.rec->last_lba,a_sector);
    x.b_read=storage::read_recovery_journal_sector(x.rec->device_index,x.b_lba,x.rec->first_lba,x.rec->last_lba,b_sector);
    x.assessment=recovery_journal::assess(x.a_read?a_sector:nullptr,x.b_read?b_sector:nullptr,disk_guid,*x.rec);
    return x;
}
static CommitResult persist(const gpt::Guid& disk_guid,const Loaded& x,const peregrinus_recovery_journal& next,recovery_journal::Copy active){
    const auto target_copy=recovery_journal::inactive_copy(active);
    if(target_copy==recovery_journal::Copy::none)return CommitResult::no_baseline;
    const uint64_t target_lba=target_copy==recovery_journal::Copy::a?x.a_lba:x.b_lba;
    uint8_t encoded[512];encode(encoded,next);
    if(!storage::write_recovery_journal_sector(x.rec->device_index,target_lba,x.rec->first_lba,x.rec->last_lba,encoded))return CommitResult::write_failed;
    if(!storage::flush_device(x.rec->device_index))return CommitResult::flush_failed;
    clear(verify_sector,512);
    if(!storage::read_recovery_journal_sector(x.rec->device_index,target_lba,x.rec->first_lba,x.rec->last_lba,verify_sector))return CommitResult::readback_failed;
    const auto verify=recovery_journal::assess(target_copy==recovery_journal::Copy::a?verify_sector:a_sector,target_copy==recovery_journal::Copy::b?verify_sector:b_sector,disk_guid,*x.rec);
    if(verify.split_brain||verify.selected!=target_copy||!record_equal(verify.record,next))return CommitResult::readback_failed;
    return CommitResult::confirmed;
}
}

Result run(const gpt::Guid& disk_guid){
    Result r{};
    if(!features::recovery_journal_read_live){serial::writeln("Muro 1.0.1 recovery journal: SKIPPED (safe default build)");return r;}
    r.attempted=true;
    const auto x=load(disk_guid);
    if(!x.rec){serial::writeln("Muro 1.0.1 recovery journal: IA_RECOVERY unavailable/too small");return r;}
    r.recovery_volume_present=true;r.a_read=x.a_read;r.b_read=x.b_read;r.assessment=x.assessment;
    serial::write("Recovery journal A/B selection: ");serial::writeln(recovery_journal::copy_name(r.assessment.selected));
    if(r.assessment.split_brain){serial::writeln("RECOVERY JOURNAL: SPLIT-BRAIN; writes blocked");return r;}
    if(!features::recovery_journal_write_test_live)return r;
    r.write_test_attempted=true;
    if(r.assessment.selected==recovery_journal::Copy::none){serial::writeln("Recovery journal write-test: no valid baseline");return r;}
    auto attempt=r.assessment.record;
    const uint8_t slot=peregrinus_rj_choose_slot(&attempt);
    if(!peregrinus_rj_prepare_attempt(&attempt,slot))return r;
    const auto first=persist(disk_guid,x,attempt,r.assessment.selected);
    if(first!=CommitResult::confirmed)return r;
    const auto x2=load(disk_guid);
    if(!x2.rec||x2.assessment.split_brain||x2.assessment.selected==recovery_journal::Copy::none)return r;
    auto success=x2.assessment.record;
    const uint64_t generation=slot==PEREGRINUS_RJ_SLOT_CURRENT?success.current_generation:success.lkg_generation;
    const uint64_t epoch=slot==PEREGRINUS_RJ_SLOT_CURRENT?success.current_epoch:success.lkg_epoch;
    if(!peregrinus_rj_mark_success(&success,slot,generation,epoch))return r;
    r.write_test_passed=persist(disk_guid,x2,success,x2.assessment.selected)==CommitResult::confirmed;
    serial::writeln(r.write_test_passed?"Recovery journal attempt+success transaction: PASS":"Recovery journal attempt+success transaction: FAIL");
    return r;
}

CommitResult confirm_boot_success(const gpt::Guid& disk_guid,unsigned char slot,unsigned long long generation,unsigned long long epoch){
    if(!features::recovery_journal_commit_live)return CommitResult::disabled;
    const auto x=load(disk_guid);
    if(!x.rec)return CommitResult::recovery_unavailable;
    if(!x.a_read&&!x.b_read)return CommitResult::read_failed;
    if(x.assessment.split_brain)return CommitResult::split_brain;
    if(x.assessment.selected==recovery_journal::Copy::none)return CommitResult::no_baseline;
    auto success=x.assessment.record;
    if(!peregrinus_rj_mark_success(&success,slot,generation,epoch))return CommitResult::mark_rejected;
    return persist(disk_guid,x,success,x.assessment.selected);
}
const char* commit_result_name(CommitResult r){
    switch(r){
        case CommitResult::disabled:return "DISABLED";
        case CommitResult::recovery_unavailable:return "RECOVERY-UNAVAILABLE";
        case CommitResult::read_failed:return "READ-FAILED";
        case CommitResult::no_baseline:return "NO-BASELINE";
        case CommitResult::split_brain:return "SPLIT-BRAIN";
        case CommitResult::mark_rejected:return "MARK-REJECTED";
        case CommitResult::write_failed:return "WRITE-FAILED";
        case CommitResult::flush_failed:return "FLUSH-FAILED";
        case CommitResult::readback_failed:return "READBACK-FAILED";
        case CommitResult::confirmed:return "CONFIRMED";
    }
    return "UNKNOWN";
}
}
