#include "root_trust.hpp"

namespace peregrinus::security::root_trust {
Slot running_slot(){
#ifdef PEREGRINUS_SLOT_LKG
    return Slot::last_known_good;
#else
    return Slot::current;
#endif
}
const char* slot_name(Slot s){return s==Slot::last_known_good?"LAST-KNOWN-GOOD":"CURRENT";}
EpochReport evaluate_epoch(uint64_t floor){
    return EpochReport{security_epoch,floor,security_epoch>=floor};
}
bool self_test(){
    const auto equal=evaluate_epoch(security_epoch);
    const auto older=evaluate_epoch(security_epoch+1);
    const auto lower=evaluate_epoch(security_epoch?security_epoch-1:0);
    return equal.accepted&&!older.accepted&&lower.accepted&&minimum_security_epoch>0;
}
}
