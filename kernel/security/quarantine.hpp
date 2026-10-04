#pragma once
#include <stddef.h>
#include <stdint.h>
namespace peregrinus::security::quarantine {
enum class Kind : uint8_t { unknown=0, driver, policy_blob, model, module };
enum class Reason : uint8_t { hash_mismatch=0, signature_failure, policy_violation, malformed, manual_hold };
struct Digest { uint8_t bytes[32]; };
struct Entry { uint64_t component_id; Kind kind; Reason reason; Digest digest; uint64_t sequence; bool active; bool digest_conflict; Digest conflict_digest; };
struct TrustedEntry { uint64_t component_id; Kind kind; Digest digest; bool active; };
// Admission is an allowlist: a component is admitted only if its ID was explicitly trusted
// with exactly this digest and it is not quarantined. Unknown components are denied.
enum class Admission : uint8_t { allow=0, deny_quarantined, deny_registry_saturated, deny_invalid_id, deny_unknown, deny_digest_mismatch };
inline constexpr size_t capacity=32;          // quarantine entries
inline constexpr size_t trusted_capacity=32;  // allowlisted component identities
class Registry {
public:
    void reset();
    // Allowlist a component identity. Refused for ID 0, a quarantined ID, a full allowlist, or an
    // ID already trusted with a different digest (no silent rebinding).
    bool trust(uint64_t component_id,Kind kind,const Digest& digest);
    bool quarantine(uint64_t component_id,Kind kind,Reason reason,const Digest& digest);
    Admission admit(uint64_t component_id,const Digest& digest) const;
    bool lookup(uint64_t component_id,Entry& out) const;
    size_t count() const{return count_;}
    size_t trusted_count() const{return trusted_count_;}
    bool saturated() const{return saturated_;}
    uint64_t rejected_invalid() const{return rejected_invalid_;}
private:
    Entry entries_[capacity]{}; size_t count_=0; uint64_t next_sequence_=1; bool saturated_=false; uint64_t rejected_invalid_=0;
    TrustedEntry trusted_[trusted_capacity]{}; size_t trusted_count_=0;
    bool quarantined(uint64_t id) const;
};
Registry& registry();
const char* admission_name(Admission);
bool self_test();
}
