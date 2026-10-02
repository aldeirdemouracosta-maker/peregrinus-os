#pragma once
#include <stdint.h>
#include <peregrinus/release_profile.h>
namespace peregrinus::security::root_trust {
enum class Slot : uint8_t { current, last_known_good };
#ifdef PEREGRINUS_SLOT_LKG
inline constexpr Slot compiled_slot=Slot::last_known_good;
inline constexpr uint64_t build_generation=PEREGRINUS_RELEASE_LKG_GENERATION;
inline constexpr uint64_t security_epoch=PEREGRINUS_RELEASE_LKG_EPOCH;
#else
inline constexpr Slot compiled_slot=Slot::current;
inline constexpr uint64_t build_generation=PEREGRINUS_RELEASE_CURRENT_GENERATION;
inline constexpr uint64_t security_epoch=PEREGRINUS_RELEASE_CURRENT_EPOCH;
#endif
inline constexpr uint64_t minimum_security_epoch=PEREGRINUS_RELEASE_MIN_SECURITY_EPOCH;
struct EpochReport{uint64_t image_epoch;uint64_t minimum_epoch;bool accepted;};
EpochReport evaluate_epoch(uint64_t floor);
Slot running_slot();
const char* slot_name(Slot);
bool self_test();
}
