#include "quarantine.hpp"
namespace peregrinus::security::quarantine {
namespace { Registry g_registry{}; bool digest_equal(const Digest&a,const Digest&b){uint8_t d=0;for(size_t i=0;i<32;++i)d|=uint8_t(a.bytes[i]^b.bytes[i]);return d==0;} }
void Registry::reset(){for(auto& e:entries_)e={};count_=0;next_sequence_=1;saturated_=false;}
bool Registry::quarantine(uint64_t id,Kind kind,Reason reason,const Digest& digest){
    if(id==0){saturated_=true;return false;}
    for(size_t i=0;i<count_;++i){if(entries_[i].active&&entries_[i].component_id==id){if(!digest_equal(entries_[i].digest,digest))entries_[i].reason=Reason::policy_violation;return true;}}
    if(count_>=capacity){saturated_=true;return false;}
    entries_[count_++]={id,kind,reason,digest,next_sequence_++,true};return true;
}
Admission Registry::admit(uint64_t id,const Digest& digest) const{
    for(size_t i=0;i<count_;++i)if(entries_[i].active&&entries_[i].component_id==id){(void)digest;return Admission::deny_quarantined;}
    return saturated_?Admission::deny_registry_saturated:Admission::allow;
}
bool Registry::lookup(uint64_t id,Entry& out) const{for(size_t i=0;i<count_;++i)if(entries_[i].active&&entries_[i].component_id==id){out=entries_[i];return true;}return false;}
Registry& registry(){return g_registry;}
const char* admission_name(Admission a){switch(a){case Admission::allow:return "ALLOW";case Admission::deny_quarantined:return "DENY-QUARANTINED";case Admission::deny_registry_saturated:return "DENY-SATURATED";}return "DENY";}
bool self_test(){Registry r;r.reset();Digest a{},b{};a.bytes[0]=1;b.bytes[0]=2;if(r.admit(7,a)!=Admission::allow)return false;if(!r.quarantine(7,Kind::driver,Reason::hash_mismatch,a))return false;if(r.admit(7,a)!=Admission::deny_quarantined||r.admit(7,b)!=Admission::deny_quarantined)return false;Entry e{};if(!r.lookup(7,e)||e.reason!=Reason::hash_mismatch)return false;if(!r.quarantine(7,Kind::driver,Reason::signature_failure,b))return false;if(!r.lookup(7,e)||e.reason!=Reason::policy_violation)return false;for(uint64_t i=8;i<8+capacity-1;++i)if(!r.quarantine(i,Kind::module,Reason::manual_hold,a))return false;if(r.quarantine(999,Kind::module,Reason::manual_hold,a))return false;if(!r.saturated()||r.admit(1000,a)!=Admission::deny_registry_saturated)return false;return true;}
}
