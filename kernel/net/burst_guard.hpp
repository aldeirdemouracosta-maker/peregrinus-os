#pragma once
#include <stddef.h>
#include <stdint.h>

namespace peregrinus::net::burst {

struct Entry { bool used; uint32_t source; uint64_t window_start; uint16_t admitted; uint16_t denied; };
inline constexpr size_t capacity=16;
inline constexpr uint64_t window_events=64;
inline constexpr uint16_t max_new_flows_per_window=8;
// Aggregate cap over all sources in the same window. The per-source table has only 16 slots, so
// an attacker rotating spoofed source addresses could otherwise reset its own counters forever.
inline constexpr uint16_t max_new_flows_total_per_window=32;
class Guard {
public:
    void reset();
    bool admit(uint32_t source,uint64_t sequence);
    uint64_t total_denied()const{return denied_total_;}
private:
    Entry entries_[capacity]{};size_t replacement_=0;uint64_t denied_total_=0;
    uint64_t global_window_start_=0;uint16_t global_admitted_=0;bool global_started_=false;
};
bool self_test();
}
