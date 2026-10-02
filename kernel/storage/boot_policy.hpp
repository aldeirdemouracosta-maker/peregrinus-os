#pragma once
#include <stdint.h>
#include "disk_probe.hpp"
#include "recovery_probe.hpp"
#include "volume.hpp"

namespace peregrinus::boot_policy {

enum class Health : uint8_t { unavailable, healthy, degraded, split_brain, no_lkg };
enum class Action : uint8_t { none, boot_readonly, recovery_readonly, halt_fail_closed };

struct Result {
    bool evaluated;
    Health health;
    Action action;
    bool gpt_trusted;
    bool protected_volumes_complete;
    bool recovery_trusted;
    bool clean_shutdown;
};

Result evaluate(const disk_probe::Result& disk,
                const volume::Catalog& volumes,
                const recovery_probe::Result& recovery);
const char* health_name(Health h);
const char* action_name(Action a);
bool self_test();

}
