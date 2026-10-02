#include "guard_policy.hpp"
namespace peregrinus::security::guard_policy {
Decision evaluate(IntegrityState integrity,const boot_policy::Result& boot){
    Decision d{integrity,Action::halt_fail_closed,false};
    if(integrity!=IntegrityState::trusted)return d;
    if(boot.action==boot_policy::Action::boot_readonly){d.action=Action::boot_readonly;return d;}
    if(boot.action==boot_policy::Action::recovery_readonly){d.action=Action::recovery_readonly;d.rollback_eligible=boot.recovery_trusted;return d;}
    return d;
}
const char* action_name(Action a){switch(a){case Action::boot_readonly:return "BOOT-READONLY";case Action::recovery_readonly:return "RECOVERY-READONLY";default:return "HALT-FAIL-CLOSED";}}
bool self_test(){
    boot_policy::Result b{};b.action=boot_policy::Action::boot_readonly;b.recovery_trusted=true;
    auto ok=evaluate(IntegrityState::trusted,b);if(ok.action!=Action::boot_readonly||ok.rollback_eligible)return false;
    auto bad=evaluate(IntegrityState::failed,b);if(bad.action!=Action::halt_fail_closed)return false;
    b.action=boot_policy::Action::recovery_readonly;auto rec=evaluate(IntegrityState::trusted,b);return rec.action==Action::recovery_readonly&&rec.rollback_eligible;
}
}
