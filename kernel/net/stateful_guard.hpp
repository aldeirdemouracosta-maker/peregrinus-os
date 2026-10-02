#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../security/firewall.hpp"

namespace peregrinus::net::stateful {

struct Flow {
    bool used;
    security::firewall::Protocol protocol;
    security::firewall::Direction origin_direction;
    uint32_t origin_src;
    uint32_t origin_dst;
    uint16_t origin_src_port;
    uint16_t origin_dst_port;
    uint64_t last_sequence;
};

inline constexpr size_t capacity = 32;
inline constexpr uint64_t event_ttl = 256;

class Guard {
public:
    void reset();
    void observe_allowed(const security::firewall::PacketV4& packet, uint64_t sequence);
    bool allow_reply(const security::firewall::PacketV4& packet, uint64_t sequence);
    size_t active_count(uint64_t sequence) const;
private:
    Flow flows_[capacity]{};
    size_t replacement_ = 0;
};

bool self_test();

}
