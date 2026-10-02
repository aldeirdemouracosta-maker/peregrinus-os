#include "boot_policy.hpp"

namespace peregrinus::boot_policy {
namespace {
static bool protected_set_complete(const volume::Catalog& c){
    if(c.duplicate_role || !c.readonly_policy) return false;
    if(c.system_index<0 || c.recovery_index<0 || c.data_index<0) return false;
    const int32_t ids[3]={c.system_index,c.recovery_index,c.data_index};
    for(unsigned i=0;i<3;++i){
        if((uint32_t)ids[i]>=c.count) return false;
        const auto& v=c.volumes[ids[i]];
        if(!v.present || !v.bound || !v.readonly || v.last_lba<v.first_lba) return false;
    }
    return true;
}
}

Result evaluate(const disk_probe::Result& disk,const volume::Catalog& vols,const recovery_probe::Result& rec){
    Result r{};
    if(!disk.attempted){ r.health=Health::unavailable; r.action=Action::none; return r; }
    r.evaluated=true;
    if(disk.split_brain){ r.health=Health::split_brain; r.action=Action::halt_fail_closed; return r; }
    r.gpt_trusted=disk.selected!=gpt::Selection::none;
    r.protected_volumes_complete=protected_set_complete(vols);
    if(!r.gpt_trusted || !r.protected_volumes_complete){ r.health=Health::degraded; r.action=Action::halt_fail_closed; return r; }
    if(!rec.attempted || !rec.recovery_volume_present){ r.health=Health::no_lkg; r.action=Action::halt_fail_closed; return r; }
    if(rec.assessment.split_brain){ r.health=Health::split_brain; r.action=Action::halt_fail_closed; return r; }
    r.recovery_trusted=rec.assessment.last_known_good && rec.assessment.selected!=recovery_anchor::Copy::none;
    r.clean_shutdown=rec.assessment.selected_clean_shutdown;
    if(!r.recovery_trusted){ r.health=Health::no_lkg; r.action=Action::halt_fail_closed; return r; }
    if(disk.degraded || rec.assessment.degraded || rec.assessment.rolling_update || !r.clean_shutdown){
        r.health=Health::degraded; r.action=Action::recovery_readonly; return r;
    }
    r.health=Health::healthy; r.action=Action::boot_readonly; return r;
}

const char* health_name(Health h){
    switch(h){
        case Health::healthy:return "HEALTHY";
        case Health::degraded:return "DEGRADED";
        case Health::split_brain:return "SPLIT-BRAIN";
        case Health::no_lkg:return "NO-LKG";
        default:return "UNAVAILABLE";
    }
}
const char* action_name(Action a){
    switch(a){
        case Action::boot_readonly:return "BOOT-READONLY";
        case Action::recovery_readonly:return "RECOVERY-READONLY";
        case Action::halt_fail_closed:return "HALT-FAIL-CLOSED";
        default:return "NONE";
    }
}

bool self_test(){
    disk_probe::Result d{}; volume::Catalog v{}; recovery_probe::Result q{};
    d.attempted=true; d.selected=gpt::Selection::primary;
    v.readonly_policy=true; v.count=3; v.system_index=0; v.recovery_index=1; v.data_index=2;
    for(unsigned i=0;i<3;++i){v.volumes[i].present=true;v.volumes[i].bound=true;v.volumes[i].readonly=true;v.volumes[i].first_lba=100+i*100;v.volumes[i].last_lba=v.volumes[i].first_lba+99;}
    q.attempted=true;q.recovery_volume_present=true;q.assessment.last_known_good=true;q.assessment.selected=recovery_anchor::Copy::first;q.assessment.selected_clean_shutdown=true;
    auto healthy=evaluate(d,v,q); if(healthy.health!=Health::healthy||healthy.action!=Action::boot_readonly)return false;
    d.degraded=true; auto degraded=evaluate(d,v,q); if(degraded.health!=Health::degraded||degraded.action!=Action::recovery_readonly)return false;
    d.degraded=false;q.assessment.last_known_good=false;q.assessment.selected=recovery_anchor::Copy::none; auto nolkg=evaluate(d,v,q); if(nolkg.health!=Health::no_lkg||nolkg.action!=Action::halt_fail_closed)return false;
    q.assessment.last_known_good=true;q.assessment.selected=recovery_anchor::Copy::first;q.assessment.split_brain=true;auto split=evaluate(d,v,q);return split.health==Health::split_brain&&split.action==Action::halt_fail_closed;
}

}
