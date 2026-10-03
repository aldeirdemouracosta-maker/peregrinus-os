// libFuzzer harness for the Muro network path: Ethernet/IPv4 parser, ARP,
// ICMP echo reply builder and the stateful datapath (rules, reply guard,
// burst guard, audit ring). ASan/UBSan catch memory and UB faults; the
// explicit checks below catch logic faults.
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "kernel/net/arp.hpp"
#include "kernel/net/icmp.hpp"
#include "kernel/net/datapath.hpp"
#include "kernel/net/ethernet_ipv4.hpp"

using namespace peregrinus;
using security::firewall::Direction;

namespace {
constexpr uint32_t local_ip = (10u << 24) | (0u << 16) | (2u << 8) | 15u;
const net::e1000::MacAddress local_mac{{0x52, 0x54, 0x00, 0x12, 0x34, 0x56}};

[[noreturn]] void fail() { abort(); }

net::Datapath& datapath() {
    static net::Datapath d;
    static bool ready = false;
    if (!ready) {
        d.reset();
        using namespace security::firewall;
        const AllowRuleV4 icmp_in{3001, true, Direction::ingress, Protocol::icmp, 0, 0, local_ip, 32, 0, 65535, 0, 65535};
        const AllowRuleV4 icmp_out{3002, true, Direction::egress, Protocol::icmp, local_ip, 32, 0, 0, 0, 65535, 0, 65535};
        if (!d.add_allow_rule(icmp_in) || !d.add_allow_rule(icmp_out)) fail();
        ready = true;
    }
    return d;
}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) return 0;
    const Direction dir = (data[0] & 1) ? Direction::egress : Direction::ingress;
    const uint8_t* frame = data + 1;
    const size_t bytes = size - 1;

    // Parser: a rejected frame must say why; an accepted one must be an
    // Ethernet frame long enough for an IPv4 header.
    const auto pr = net::parse_ethernet_ipv4(dir, frame, bytes);
    if (pr.accepted != (pr.status == net::ParseStatus::ok)) fail();
    if (pr.accepted && bytes < 14 + 20) fail();

    // ARP: any reply we build must itself parse as an ARP reply from us.
    net::arp::Packet ap{};
    if (net::arp::parse(frame, bytes, ap) == net::arp::Status::ok) {
        uint8_t out[64];
        const size_t n = net::arp::build_reply(ap, local_mac, local_ip, out, sizeof(out));
        if (n > sizeof(out)) fail();
        if (n) {
            net::arp::Packet rp{};
            if (net::arp::parse(out, n, rp) != net::arp::Status::ok) fail();
            if (rp.opcode != 2 || rp.sender_ip != local_ip) fail();
        }
    }

    // ICMP: an echo reply must fit the buffer and be a valid IPv4 frame
    // (header checksum included) that the parser accepts on egress.
    uint8_t reply[1514];
    const size_t rn = net::icmp::build_echo_reply(frame, bytes, local_mac, local_ip, reply, sizeof(reply));
    if (rn > sizeof(reply)) fail();
    if (rn) {
        const auto rr = net::parse_ethernet_ipv4(Direction::egress, reply, rn);
        if (!rr.accepted) fail();
    }

    // Datapath keeps state across inputs (stateful/burst guards, audit ring).
    auto& d = datapath();
    const uint64_t before = d.allowed() + d.denied();
    (void)d.inspect(dir, frame, bytes);
    if (d.allowed() + d.denied() != before + 1) fail();
    if (d.audit_count() > net::datapath_audit_capacity) fail();
    return 0;
}
