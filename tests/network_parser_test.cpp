#include <cstdio>
#include "net/ethernet_ipv4.hpp"
#include "net/datapath.hpp"
#include "net/nic_probe.hpp"

int main() {
    using namespace peregrinus;
    if (!net::ethernet_ipv4_self_test()) { std::puts("FAIL: Ethernet/IPv4 parser"); return 1; }
    if (!net::nic::self_test()) { std::puts("FAIL: NIC classifier"); return 2; }
    // Single audit point: every decision of the datapath (parse reject included) is recorded,
    // the ring stays bounded and returns events oldest-first.
    net::Datapath d; d.reset();
    uint8_t junk[20]{};
    for (size_t i = 0; i < net::datapath_audit_capacity + 7; ++i) d.inspect(security::firewall::Direction::ingress, junk, sizeof junk);
    if (d.audit_count() != net::datapath_audit_capacity) { std::puts("FAIL: audit ring not bounded"); return 3; }
    net::DatapathAudit first{}, last{};
    if (!d.audit_get_oldest(0, first) || !d.audit_get_oldest(net::datapath_audit_capacity - 1, last) || last.sequence <= first.sequence) { std::puts("FAIL: audit ring order"); return 4; }
    if (first.decision.reason != net::DatapathReason::parse_reject) { std::puts("FAIL: parse rejects must be audited"); return 5; }
    return 0;
}
