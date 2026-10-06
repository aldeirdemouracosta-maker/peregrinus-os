#include "quarantine.hpp"
namespace peregrinus::security::quarantine {
namespace { Registry g_registry{}; bool digest_equal(const Digest&a,const Digest&b){uint8_t d=0;for(size_t i=0;i<32;++i)d|=uint8_t(a.bytes[i]^b.bytes[i]);return d==0;} }
void Registry::reset(){for(auto& e:entries_)e={};count_=0;next_sequence_=1;saturated_=false;rejected_invalid_=0;for(auto& t:trusted_)t={};trusted_count_=0;}
bool Registry::quarantined(uint64_t id) const{for(size_t i=0;i<count_;++i)if(entries_[i].active&&entries_[i].component_id==id)return true;return false;}
bool Registry::trust(uint64_t id,Kind kind,const Digest& digest){
    if(id==0){++rejected_invalid_;return false;}
    if(quarantined(id))return false;
    for(size_t i=0;i<trusted_count_;++i)if(trusted_[i].active&&trusted_[i].component_id==id)return digest_equal(trusted_[i].digest,digest);
    if(trusted_count_>=trusted_capacity)return false;
    trusted_[trusted_count_++]={id,kind,digest,true};return true;
}
bool Registry::quarantine(uint64_t id,Kind kind,Reason reason,const Digest& digest){
    if(id==0){++rejected_invalid_;return false;}
    for(size_t i=0;i<count_;++i){if(entries_[i].active&&entries_[i].component_id==id){if(!digest_equal(entries_[i].digest,digest)){entries_[i].reason=Reason::policy_violation;entries_[i].digest_conflict=true;entries_[i].conflict_digest=digest;}return true;}}
    if(count_>=capacity){saturated_=true;return false;}
    entries_[count_++]={id,kind,reason,digest,next_sequence_++,true,false,{}};return true;
}
Admission Registry::admit(uint64_t id,const Digest& digest) const{
    if(id==0)return Admission::deny_invalid_id;
    if(quarantined(id))return Admission::deny_quarantined;
    if(saturated_)return Admission::deny_registry_saturated;
    for(size_t i=0;i<trusted_count_;++i)if(trusted_[i].active&&trusted_[i].component_id==id)
        return digest_equal(trusted_[i].digest,digest)?Admission::allow:Admission::deny_digest_mismatch;
    return Admission::deny_unknown;
}
bool Registry::lookup(uint64_t id,Entry& out) const{for(size_t i=0;i<count_;++i)if(entries_[i].active&&entries_[i].component_id==id){out=entries_[i];return true;}return false;}
Registry& registry(){return g_registry;}
const char* admission_name(Admission a){switch(a){case Admission::allow:return "ALLOW";case Admission::deny_quarantined:return "DENY-QUARANTINED";case Admission::deny_registry_saturated:return "DENY-SATURATED";case Admission::deny_invalid_id:return "DENY-INVALID-ID";case Admission::deny_unknown:return "DENY-UNKNOWN";case Admission::deny_digest_mismatch:return "DENY-DIGEST-MISMATCH";}return "DENY";}
bool self_test(){
    Registry r;r.reset();Digest a{},b{};a.bytes[0]=1;b.bytes[0]=2;
    // Allowlist semantics: unknown denied; trusted+matching digest allowed; wrong digest denied.
    if(r.admit(7,a)!=Admission::deny_unknown)return false;
    if(!r.trust(7,Kind::driver,a)||r.admit(7,a)!=Admission::allow||r.admit(7,b)!=Admission::deny_digest_mismatch)return false;
    if(!r.trust(7,Kind::driver,a)||r.trust(7,Kind::driver,b))return false;       // idempotent, no rebinding
    if(r.trust(0,Kind::driver,a))return false;
    // Quarantine overrides trust.
    if(!r.quarantine(7,Kind::driver,Reason::hash_mismatch,a))return false;
    if(r.admit(7,a)!=Admission::deny_quarantined||r.admit(7,b)!=Admission::deny_quarantined)return false;
    if(!r.trust(9,Kind::driver,a)||!r.quarantine(9,Kind::driver,Reason::manual_hold,a))return false;
    if(r.trust(9,Kind::driver,a)||r.admit(9,a)!=Admission::deny_quarantined)return false;  // quarantined IDs cannot be re-trusted
    Entry e{};if(!r.lookup(7,e)||e.reason!=Reason::hash_mismatch)return false;
    if(!r.quarantine(7,Kind::driver,Reason::signature_failure,b))return false;
    if(!r.lookup(7,e)||e.reason!=Reason::policy_violation||!e.digest_conflict||!digest_equal(e.digest,a)||!digest_equal(e.conflict_digest,b))return false;
    if(r.quarantine(0,Kind::module,Reason::manual_hold,a)||r.saturated())return false;
    if(r.admit(0,a)!=Admission::deny_invalid_id||r.admit(8,a)!=Admission::deny_unknown)return false;
    // Saturation of the quarantine registry denies everything, trusted components included.
    if(!r.trust(8,Kind::module,a)||r.admit(8,a)!=Admission::allow)return false;
    for(uint64_t i=100;r.count()<capacity;++i)if(!r.quarantine(i,Kind::module,Reason::manual_hold,a))return false;
    if(r.quarantine(999,Kind::module,Reason::manual_hold,a))return false;
    if(!r.saturated()||r.admit(8,a)!=Admission::deny_registry_saturated)return false;
    // Allowlist capacity is bounded and fails closed.
    Registry t;t.reset();for(uint64_t i=1;i<=trusted_capacity;++i)if(!t.trust(i,Kind::module,a))return false;
    return !t.trust(trusted_capacity+1,Kind::module,a)&&t.admit(trusted_capacity+1,a)==Admission::deny_unknown;
}
}
