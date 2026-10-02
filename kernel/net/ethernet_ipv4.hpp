#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../security/firewall.hpp"

namespace peregrinus::net {

enum class ParseStatus : uint8_t {
    ok = 0,
    frame_too_short,
    unsupported_ethertype,
    vlan_unsupported,
    ipv4_too_short,
    ipv4_version_invalid,
    ipv4_ihl_invalid,
    ipv4_options_unsupported,
    ipv4_total_length_invalid,
    ipv4_mtu_unsupported,
    ipv4_checksum_invalid,
    unsupported_protocol,
    transport_too_short,
    tcp_header_invalid,
    udp_length_invalid,
};

struct ParseResult {
    bool accepted;
    ParseStatus status;
    uint16_t ether_type;
    security::firewall::PacketV4 packet;
};

ParseResult parse_ethernet_ipv4(security::firewall::Direction direction,
                                const uint8_t* frame,
                                size_t frame_size);
const char* parse_status_name(ParseStatus status);
bool ethernet_ipv4_self_test();

}
