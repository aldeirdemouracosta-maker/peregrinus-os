#include "guard_policy.hpp"
namespace peregrinus::security::guard_policy {
Decision evaluate(IntegrityState integrity,const boot_policy::Result& boot,bool storage_policy_required){
    Decision d{integrity,Action::halt_fail_closed,false};
    if(integrity!=IntegrityState::trusted)return d;
    if(boot.action==boot_policy::Action::boot_readonly){d.action=Action::boot_readonly;return d;}
    if(boot.action==boot_policy::Action::recovery_readonly){d.action=Action::recovery_readonly;d.rollback_eligible=boot.recovery_trusted;return d;}
    if(boot.action==boot_policy::Action::none&&!boot.evaluated&&!storage_policy_required){d.action=Action::boot_passive;return d;}
    return d;
}
const char* action_name(Action a){switch(a){case Action::boot_readonly:return "BOOT-READONLY";case Action::recovery_readonly:return "RECOVERY-READONLY";case Action::boot_passive:return "BOOT-PASSIVE (no storage path)";default:return "HALT-FAIL-CLOSED";}}
bool self_test(){
    boot_policy::Result b{};b.action=boot_policy::Action::boot_readonly;b.recovery_trusted=true;
    auto ok=evaluate(IntegrityState::trusted,b,true);if(ok.action!=Action::boot_readonly||ok.rollback_eligible)return false;
    auto bad=evaluate(IntegrityState::failed,b,true);if(bad.action!=Action::halt_fail_closed)return false;
    b.action=boot_policy::Action::recovery_readonly;auto rec=evaluate(IntegrityState::trusted,b,true);if(rec.action!=Action::recovery_readonly||!rec.rollback_eligible)return false;
    // Passive builds (no storage path): an unevaluated disk policy must not halt the boot...
    boot_policy::Result none{};
    if(evaluate(IntegrityState::trusted,none,false).action!=Action::boot_passive)return false;
    // ...but integrity failure still halts, and storage builds still fail closed.
    if(evaluate(IntegrityState::failed,none,false).action!=Action::halt_fail_closed)return false;
    if(evaluate(IntegrityState::unverified,none,false).action!=Action::halt_fail_closed)return false;
    if(evaluate(IntegrityState::trusted,none,true).action!=Action::halt_fail_closed)return false;
    boot_policy::Result halted{};halted.evaluated=true;halted.action=boot_policy::Action::halt_fail_closed;
    if(evaluate(IntegrityState::trusted,halted,false).action!=Action::halt_fail_closed)return false;
    boot_policy::Result inconsistent{};inconsistent.evaluated=true;
    return evaluate(IntegrityState::trusted,inconsistent,false).action==Action::halt_fail_closed;
}
}
