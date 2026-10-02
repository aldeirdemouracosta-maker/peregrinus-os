#pragma once
#include <stddef.h>
#include <stdint.h>
#include "e1000.hpp"

namespace peregrinus::net::arp {

enum class Status : uint8_t { ok=0, too_short, not_arp, unsupported, invalid };
struct Packet {
    uint16_t opcode;
    e1000::MacAddress sender_mac;
    uint32_t sender_ip;
    e1000::MacAddress target_mac;
    uint32_t target_ip;
};

Status parse(const uint8_t* frame, size_t bytes, Packet& out);
size_t build_reply(const Packet& request, const e1000::MacAddress& local_mac, uint32_t local_ip,
                   uint8_t* out, size_t capacity);
bool self_test();

}
