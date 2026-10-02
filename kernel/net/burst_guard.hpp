#pragma once
#include <stddef.h>
#include <stdint.h>

namespace peregrinus::net::burst {

struct Entry { bool used; uint32_t source; uint64_t window_start; uint16_t admitted; uint16_t denied; };
inline constexpr size_t capacity=16;
inline constexpr uint64_t window_events=64;
inline constexpr uint16_t max_new_flows_per_window=8;
class Guard {
public:
    void reset();
    bool admit(uint32_t source,uint64_t sequence);
    uint64_t total_denied()const{return denied_total_;}
private:
    Entry entries_[capacity]{};size_t replacement_=0;uint64_t denied_total_=0;
};
bool self_test();
}
