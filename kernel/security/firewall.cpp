#include "firewall.hpp"

namespace peregrinus::security::firewall {
namespace {

bool valid_direction(Direction d) {
    return d == Direction::ingress || d == Direction::egress;
}

bool valid_protocol(Protocol p) {
    return p == Protocol::icmp || p == Protocol::tcp || p == Protocol::udp;
}

uint32_t prefix_mask(uint8_t prefix) {
    if (prefix == 0) return 0u;
    return 0xffffffffu << (32u - prefix);
}

bool prefix_matches(uint32_t address, uint32_t network, uint8_t prefix) {
    if (prefix == 0) return true;
    if (prefix > 32) return false;
    const uint32_t mask = prefix_mask(prefix);
    return (address & mask) == (network & mask);
}

bool port_in_range(uint16_t port, uint16_t first, uint16_t last) {
    return port >= first && port <= last;
}

bool valid_rule(const AllowRuleV4& r) {
    if (!r.enabled || r.id == 0 || !valid_direction(r.direction)) return false;
    if (r.protocol != Protocol::any && !valid_protocol(r.protocol)) return false;
    if (r.src_prefix > 32 || r.dst_prefix > 32) return false;
    if (r.src_port_first > r.src_port_last || r.dst_port_first > r.dst_port_last) return false;
    if (r.protocol == Protocol::icmp || r.protocol == Protocol::any) {
        if (r.src_port_first != 0 || r.src_port_last != 65535) return false;
        if (r.dst_port_first != 0 || r.dst_port_last != 65535) return false;
    }
    return true;
}

bool rule_matches(const AllowRuleV4& r, const PacketV4& p) {
    if (!r.enabled || r.direction != p.direction) return false;
    if (r.protocol != Protocol::any && r.protocol != p.protocol) return false;
    if (!prefix_matches(p.src, r.src_network, r.src_prefix)) return false;
    if (!prefix_matches(p.dst, r.dst_network, r.dst_prefix)) return false;
    if (p.protocol == Protocol::tcp || p.protocol == Protocol::udp) {
        if (!port_in_range(p.src_port, r.src_port_first, r.src_port_last)) return false;
        if (!port_in_range(p.dst_port, r.dst_port_first, r.dst_port_last)) return false;
    }
    return true;
}

bool packet_valid(const PacketV4& p) {
    return valid_direction(p.direction) && valid_protocol(p.protocol);
}

}

uint32_t ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (static_cast<uint32_t>(a) << 24) |
           (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(c) << 8) |
           static_cast<uint32_t>(d);
}

void Engine::reset() {
    rule_count_ = 0;
    audit_head_ = 0;
    audit_count_ = 0;
    next_sequence_ = 1;
    for (size_t i = 0; i < max_rules; ++i) rules_[i] = {};
    for (size_t i = 0; i < audit_capacity; ++i) audit_[i] = {};
}

bool Engine::add_allow_rule(const AllowRuleV4& rule) {
    if (rule_count_ >= max_rules || !valid_rule(rule)) return false;
    for (size_t i = 0; i < rule_count_; ++i) {
        if (rules_[i].id == rule.id) return false;
    }
    rules_[rule_count_++] = rule;
    return true;
}

void Engine::audit(const PacketV4& packet, const Decision& decision) {
    AuditEvent event{};
    event.sequence = next_sequence_++;
    event.decision = decision;
    event.packet = packet;
    audit_[audit_head_] = event;
    audit_head_ = (audit_head_ + 1) % audit_capacity;
    if (audit_count_ < audit_capacity) ++audit_count_;
}

Decision Engine::evaluate(const PacketV4& packet) {
    Decision decision{Verdict::deny, Reason::invalid_packet, 0};
    if (!packet_valid(packet)) {
        audit(packet, decision);
        return decision;
    }
    if (packet.fragmented) {
        decision.reason = Reason::fragment_unsupported;
        audit(packet, decision);
        return decision;
    }
    for (size_t i = 0; i < rule_count_; ++i) {
        if (rule_matches(rules_[i], packet)) {
            decision = {Verdict::allow, Reason::allow_rule, rules_[i].id};
            audit(packet, decision);
            return decision;
        }
    }
    decision.reason = Reason::default_deny;
    audit(packet, decision);
    return decision;
}

bool Engine::audit_get_oldest(size_t index, AuditEvent& out) const {
    if (index >= audit_count_) return false;
    const size_t oldest = (audit_head_ + audit_capacity - audit_count_) % audit_capacity;
    const size_t slot = (oldest + index) % audit_capacity;
    out = audit_[slot];
    return true;
}

const char* verdict_name(Verdict verdict) {
    return verdict == Verdict::allow ? "ALLOW" : "DENY";
}

const char* reason_name(Reason reason) {
    switch (reason) {
        case Reason::invalid_packet: return "INVALID-PACKET";
        case Reason::fragment_unsupported: return "FRAGMENT-UNSUPPORTED";
        case Reason::allow_rule: return "ALLOWLIST-RULE";
        case Reason::default_deny: return "DEFAULT-DENY";
    }
    return "UNKNOWN";
}

bool self_test() {
    Engine e;
    e.reset();

    const PacketV4 ssh_out{Direction::egress, Protocol::tcp, ipv4(10, 0, 0, 10), ipv4(10, 0, 0, 20), 40000, 22, false, 0, 0, 0};
    auto d = e.evaluate(ssh_out);
    if (d.verdict != Verdict::deny || d.reason != Reason::default_deny) return false;

    const AllowRuleV4 allow_ssh{
        1001, true, Direction::egress, Protocol::tcp,
        ipv4(10, 0, 0, 0), 24, ipv4(10, 0, 0, 20), 32,
        1024, 65535, 22, 22
    };
    if (!e.add_allow_rule(allow_ssh)) return false;
    if (e.add_allow_rule(allow_ssh)) return false;

    d = e.evaluate(ssh_out);
    if (d.verdict != Verdict::allow || d.reason != Reason::allow_rule || d.rule_id != 1001) return false;

    PacketV4 wrong_dst = ssh_out;
    wrong_dst.dst = ipv4(10, 0, 0, 21);
    d = e.evaluate(wrong_dst);
    if (d.verdict != Verdict::deny || d.reason != Reason::default_deny) return false;

    PacketV4 fragment = ssh_out;
    fragment.fragmented = true;
    d = e.evaluate(fragment);
    if (d.verdict != Verdict::deny || d.reason != Reason::fragment_unsupported) return false;

    const AllowRuleV4 invalid_any{
        2000, true, Direction::ingress, Protocol::any,
        0, 0, 0, 0, 0, 10, 0, 65535
    };
    if (e.add_allow_rule(invalid_any)) return false;

    for (size_t i = 0; i < audit_capacity + 5; ++i) {
        e.evaluate(wrong_dst);
    }
    if (e.audit_count() != audit_capacity) return false;
    AuditEvent first{};
    AuditEvent last{};
    if (!e.audit_get_oldest(0, first)) return false;
    if (!e.audit_get_oldest(audit_capacity - 1, last)) return false;
    if (last.sequence <= first.sequence) return false;
    return true;
}

}
