#pragma once
#include <stdint.h>

namespace peregrinus::security::watchdog {
enum class Stage : uint8_t { none=0, boot_entry=1, cpu_ready=2, memory_ready=3, integrity_ready=4, devices_ready=5, recovery_ready=6, policy_ready=7, complete=8 };
struct State { Stage current; bool tripped; uint32_t checkpoints; };
void reset();
bool checkpoint(Stage next);
const State& state();
const char* stage_name(Stage s);
bool self_test();
}
