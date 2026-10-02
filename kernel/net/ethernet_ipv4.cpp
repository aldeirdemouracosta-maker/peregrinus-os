#include "ethernet_ipv4.hpp"

namespace peregrinus::net {
namespace {

constexpr size_t ethernet_header_size = 14;
constexpr uint16_t ethertype_ipv4 = 0x0800;
constexpr uint16_t ethertype_vlan = 0x8100;
constexpr uint16_t ethertype_vlan_provider = 0x88a8;

uint16_t read_be16(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

uint32_t read_be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) |
           static_cast<uint32_t>(p[3]);
}

void write_be16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v >> 8);
    p[1] = static_cast<uint8_t>(v);
}

uint16_t checksum_value(const uint8_t* data, size_t size) {
    uint32_t sum = 0;
    size_t i = 0;
    while (i + 1 < size) {
        sum += read_be16(data + i);
        i += 2;
    }
    if (i < size) sum += static_cast<uint16_t>(data[i]) << 8;
    while (sum >> 16) sum = (sum & 0xffffu) + (sum >> 16);
    return static_cast<uint16_t>(~sum);
}

bool ipv4_checksum_valid(const uint8_t* header, size_t size) {
    uint32_t sum = 0;
    for (size_t i = 0; i < size; i += 2) {
        sum += read_be16(header + i);
    }
    while (sum >> 16) sum = (sum & 0xffffu) + (sum >> 16);
    return static_cast<uint16_t>(sum) == 0xffffu;
}

ParseResult reject(ParseStatus status, uint16_t ether_type = 0) {
    ParseResult out{};
    out.accepted = false;
    out.status = status;
    out.ether_type = ether_type;
    return out;
}

size_t build_ipv4_udp_frame(uint8_t* frame, size_t capacity,
                            uint32_t src, uint32_t dst,
                            uint16_t src_port, uint16_t dst_port,
                            bool fragmented) {
    constexpr size_t ip_size = 20;
    constexpr size_t udp_size = 8;
    constexpr size_t total = ethernet_header_size + ip_size + udp_size;
    if (capacity < total) return 0;
    for (size_t i = 0; i < total; ++i) frame[i] = 0;
    frame[12] = 0x08;
    frame[13] = 0x00;
    uint8_t* ip = frame + ethernet_header_size;
    ip[0] = 0x45;
    write_be16(ip + 2, static_cast<uint16_t>(ip_size + udp_size));
    write_be16(ip + 4, 0x1234);
    write_be16(ip + 6, fragmented ? 0x2000 : 0x0000);
    ip[8] = 64;
    ip[9] = 17;
    ip[12] = static_cast<uint8_t>(src >> 24);
    ip[13] = static_cast<uint8_t>(src >> 16);
    ip[14] = static_cast<uint8_t>(src >> 8);
    ip[15] = static_cast<uint8_t>(src);
    ip[16] = static_cast<uint8_t>(dst >> 24);
    ip[17] = static_cast<uint8_t>(dst >> 16);
    ip[18] = static_cast<uint8_t>(dst >> 8);
    ip[19] = static_cast<uint8_t>(dst);
    write_be16(ip + 10, checksum_value(ip, ip_size));
    uint8_t* udp = ip + ip_size;
    write_be16(udp + 0, src_port);
    write_be16(udp + 2, dst_port);
    write_be16(udp + 4, udp_size);
    return total;
}

}

ParseResult parse_ethernet_ipv4(security::firewall::Direction direction,
                                const uint8_t* frame,
                                size_t frame_size) {
    if (!frame || frame_size < ethernet_header_size) return reject(ParseStatus::frame_too_short);
    const uint16_t ether_type = read_be16(frame + 12);
    if (ether_type == ethertype_vlan || ether_type == ethertype_vlan_provider) {
        return reject(ParseStatus::vlan_unsupported, ether_type);
    }
    if (ether_type != ethertype_ipv4) return reject(ParseStatus::unsupported_ethertype, ether_type);

    const uint8_t* ip = frame + ethernet_header_size;
    const size_t ip_available = frame_size - ethernet_header_size;
    if (ip_available < 20) return reject(ParseStatus::ipv4_too_short, ether_type);
    if ((ip[0] >> 4) != 4) return reject(ParseStatus::ipv4_version_invalid, ether_type);
    const size_t ihl = static_cast<size_t>(ip[0] & 0x0f) * 4;
    if (ihl < 20 || ihl > 60 || ihl > ip_available || (ihl & 3u) != 0) {
        return reject(ParseStatus::ipv4_ihl_invalid, ether_type);
    }
    if (ihl != 20) return reject(ParseStatus::ipv4_options_unsupported, ether_type);
    const uint16_t total_length = read_be16(ip + 2);
    if (total_length < ihl || total_length > ip_available) {
        return reject(ParseStatus::ipv4_total_length_invalid, ether_type);
    }
    if (total_length > 1500) return reject(ParseStatus::ipv4_mtu_unsupported, ether_type);
    if (!ipv4_checksum_valid(ip, ihl)) return reject(ParseStatus::ipv4_checksum_invalid, ether_type);

    security::firewall::Protocol protocol{};
    switch (ip[9]) {
        case 1: protocol = security::firewall::Protocol::icmp; break;
        case 6: protocol = security::firewall::Protocol::tcp; break;
        case 17: protocol = security::firewall::Protocol::udp; break;
        default: return reject(ParseStatus::unsupported_protocol, ether_type);
    }

    security::firewall::PacketV4 packet{};
    packet.direction = direction;
    packet.protocol = protocol;
    packet.src = read_be32(ip + 12);
    packet.dst = read_be32(ip + 16);
    const uint16_t fragment = read_be16(ip + 6);
    packet.fragmented = (fragment & 0x3fffu) != 0;

    const uint8_t* transport = ip + ihl;
    const size_t transport_size = total_length - ihl;
    if (protocol == security::firewall::Protocol::tcp) {
        if (transport_size < 20) return reject(ParseStatus::transport_too_short, ether_type);
        const size_t tcp_header = static_cast<size_t>(transport[12] >> 4) * 4;
        if (tcp_header < 20 || tcp_header > transport_size) return reject(ParseStatus::tcp_header_invalid, ether_type);
        packet.src_port = read_be16(transport + 0);
        packet.dst_port = read_be16(transport + 2);
        packet.tcp_flags = transport[13];
    } else if (protocol == security::firewall::Protocol::udp) {
        if (transport_size < 8) return reject(ParseStatus::transport_too_short, ether_type);
        const uint16_t udp_length = read_be16(transport + 4);
        if (udp_length < 8 || udp_length > transport_size) return reject(ParseStatus::udp_length_invalid, ether_type);
        packet.src_port = read_be16(transport + 0);
        packet.dst_port = read_be16(transport + 2);
    } else {
        if (transport_size < 4) return reject(ParseStatus::transport_too_short, ether_type);
        packet.src_port = 0;
        packet.dst_port = 0;
        packet.icmp_type = transport[0];
        packet.icmp_code = transport[1];
    }

    ParseResult out{};
    out.accepted = true;
    out.status = ParseStatus::ok;
    out.ether_type = ether_type;
    out.packet = packet;
    return out;
}

const char* parse_status_name(ParseStatus status) {
    switch (status) {
        case ParseStatus::ok: return "OK";
        case ParseStatus::frame_too_short: return "FRAME-TOO-SHORT";
        case ParseStatus::unsupported_ethertype: return "UNSUPPORTED-ETHERTYPE";
        case ParseStatus::vlan_unsupported: return "VLAN-UNSUPPORTED";
        case ParseStatus::ipv4_too_short: return "IPV4-TOO-SHORT";
        case ParseStatus::ipv4_version_invalid: return "IPV4-VERSION";
        case ParseStatus::ipv4_ihl_invalid: return "IPV4-IHL";
        case ParseStatus::ipv4_options_unsupported: return "IPV4-OPTIONS-UNSUPPORTED";
        case ParseStatus::ipv4_total_length_invalid: return "IPV4-TOTAL-LENGTH";
        case ParseStatus::ipv4_mtu_unsupported: return "IPV4-MTU-UNSUPPORTED";
        case ParseStatus::ipv4_checksum_invalid: return "IPV4-CHECKSUM";
        case ParseStatus::unsupported_protocol: return "UNSUPPORTED-PROTOCOL";
        case ParseStatus::transport_too_short: return "TRANSPORT-TOO-SHORT";
        case ParseStatus::tcp_header_invalid: return "TCP-HEADER";
        case ParseStatus::udp_length_invalid: return "UDP-LENGTH";
    }
    return "UNKNOWN";
}

bool ethernet_ipv4_self_test() {
    uint8_t frame[64]{};
    const uint32_t src = security::firewall::ipv4(192, 168, 1, 10);
    const uint32_t dst = security::firewall::ipv4(1, 1, 1, 1);
    const size_t size = build_ipv4_udp_frame(frame, sizeof(frame), src, dst, 53000, 53, false);
    if (size == 0) return false;

    auto r = parse_ethernet_ipv4(security::firewall::Direction::egress, frame, size);
    if (!r.accepted || r.status != ParseStatus::ok || r.ether_type != ethertype_ipv4) return false;
    if (r.packet.src != src || r.packet.dst != dst || r.packet.src_port != 53000 || r.packet.dst_port != 53) return false;
    if (r.packet.protocol != security::firewall::Protocol::udp || r.packet.fragmented) return false;

    uint8_t bad_checksum[64]{};
    for (size_t i = 0; i < size; ++i) bad_checksum[i] = frame[i];
    bad_checksum[ethernet_header_size + 8] ^= 1;
    r = parse_ethernet_ipv4(security::firewall::Direction::egress, bad_checksum, size);
    if (r.accepted || r.status != ParseStatus::ipv4_checksum_invalid) return false;

    uint8_t vlan[64]{};
    for (size_t i = 0; i < size; ++i) vlan[i] = frame[i];
    vlan[12] = 0x81;
    vlan[13] = 0x00;
    r = parse_ethernet_ipv4(security::firewall::Direction::egress, vlan, size);
    if (r.accepted || r.status != ParseStatus::vlan_unsupported) return false;

    uint8_t options[68]{};
    for (size_t i = 0; i < size; ++i) options[i] = frame[i];
    uint8_t* option_ip = options + ethernet_header_size;
    option_ip[0] = 0x46;
    write_be16(option_ip + 2, 32);
    write_be16(option_ip + 10, 0);
    write_be16(option_ip + 10, checksum_value(option_ip, 24));
    r = parse_ethernet_ipv4(security::firewall::Direction::egress, options, size + 4);
    if (r.accepted || r.status != ParseStatus::ipv4_options_unsupported) return false;

    const size_t fragmented_size = build_ipv4_udp_frame(frame, sizeof(frame), src, dst, 53000, 53, true);
    r = parse_ethernet_ipv4(security::firewall::Direction::egress, frame, fragmented_size);
    if (!r.accepted || !r.packet.fragmented) return false;

    r = parse_ethernet_ipv4(security::firewall::Direction::egress, frame, 13);
    if (r.accepted || r.status != ParseStatus::frame_too_short) return false;
    return true;
}

}
