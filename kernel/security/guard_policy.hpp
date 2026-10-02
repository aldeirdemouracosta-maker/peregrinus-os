#pragma once
#include <stdint.h>
#include "../storage/boot_policy.hpp"

namespace peregrinus::security::guard_policy {
enum class IntegrityState : uint8_t { unverified, trusted, failed };
enum class Action : uint8_t { halt_fail_closed, boot_readonly, recovery_readonly };
struct Decision { IntegrityState integrity; Action action; bool rollback_eligible; };
Decision evaluate(IntegrityState integrity,const boot_policy::Result& boot);
const char* action_name(Action a);
bool self_test();
}
