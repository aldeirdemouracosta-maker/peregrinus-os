#pragma once
#include <stddef.h>
#include <stdint.h>
#include "e1000.hpp"

namespace peregrinus::net::icmp {
size_t build_echo_reply(const uint8_t* request, size_t request_bytes,
                        const e1000::MacAddress& local_mac, uint32_t local_ip,
                        uint8_t* out, size_t capacity);
bool self_test();
}
