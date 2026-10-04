#pragma once
#include <stdint.h>
#include "../storage/boot_policy.hpp"

namespace peregrinus::security::guard_policy {
enum class IntegrityState : uint8_t { unverified, trusted, failed };
// boot_passive: the build has no storage path at all (SAFE / e1000 qualification), so there is
// no disk policy to evaluate; the kernel continues without touching persistent media.
enum class Action : uint8_t { halt_fail_closed, boot_readonly, recovery_readonly, boot_passive };
struct Decision { IntegrityState integrity; Action action; bool rollback_eligible; };
// storage_policy_required: true when the build can read/write recovery metadata. In that case an
// unevaluated disk policy is a failure and halts; otherwise it maps to boot_passive.
Decision evaluate(IntegrityState integrity,const boot_policy::Result& boot,bool storage_policy_required);
const char* action_name(Action a);
bool self_test();
}
