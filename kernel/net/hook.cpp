#include "hook.hpp"

namespace peregrinus::net {
namespace {

void write_be16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v >> 8);
    p[1] = static_cast<uint8_t>(v);
}

uint16_t read_be16(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

uint16_t checksum_value(const uint8_t* data, size_t size) {
    uint32_t sum = 0;
    for (size_t i = 0; i < size; i += 2) sum += read_be16(data + i);
    while (sum >> 16) sum = (sum & 0xffffu) + (sum >> 16);
    return static_cast<uint16_t>(~sum);
}

size_t make_udp(uint8_t* frame, size_t capacity, uint32_t src, uint32_t dst, uint16_t sport, uint16_t dport) {
    constexpr size_t total = 14 + 20 + 8;
    if (capacity < total) return 0;
    for (size_t i = 0; i < total; ++i) frame[i] = 0;
    frame[12] = 0x08;
    frame[13] = 0x00;
    uint8_t* ip = frame + 14;
    ip[0] = 0x45;
    write_be16(ip + 2, 28);
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
    write_be16(ip + 10, checksum_value(ip, 20));
    uint8_t* udp = ip + 20;
    write_be16(udp + 0, sport);
    write_be16(udp + 2, dport);
    write_be16(udp + 4, 8);
    return total;
}

}

void Hook::audit(security::firewall::Direction direction, size_t frame_size, uint16_t ether_type, const HookDecision& decision) {
    HookAuditEvent event{};
    event.sequence = next_sequence_++;
    event.direction = direction;
    event.frame_size = frame_size;
    event.ether_type = ether_type;
    event.decision = decision;
    audit_[audit_head_] = event;
    audit_head_ = (audit_head_ + 1) % hook_audit_capacity;
    if (audit_count_ < hook_audit_capacity) ++audit_count_;
}

HookDecision Hook::inspect(security::firewall::Direction direction, const uint8_t* frame, size_t frame_size) {
    const ParseResult parsed = parse_ethernet_ipv4(direction, frame, frame_size);
    HookDecision out{security::firewall::Verdict::deny, HookReason::parse_reject, parsed.status, 0};
    if (!parsed.accepted) {
        audit(direction, frame_size, parsed.ether_type, out);
        return out;
    }

    const auto fw = engine_->evaluate(parsed.packet);
    out.verdict = fw.verdict;
    out.reason = fw.verdict == security::firewall::Verdict::allow ? HookReason::firewall_allow : HookReason::firewall_deny;
    out.parse_status = ParseStatus::ok;
    out.rule_id = fw.rule_id;
    audit(direction, frame_size, parsed.ether_type, out);
    return out;
}

bool Hook::audit_get_oldest(size_t index, HookAuditEvent& out) const {
    if (index >= audit_count_) return false;
    const size_t oldest = (audit_head_ + hook_audit_capacity - audit_count_) % hook_audit_capacity;
    out = audit_[(oldest + index) % hook_audit_capacity];
    return true;
}

const char* hook_reason_name(HookReason reason) {
    switch (reason) {
        case HookReason::parse_reject: return "PARSE-REJECT";
        case HookReason::firewall_allow: return "FIREWALL-ALLOW";
        case HookReason::firewall_deny: return "FIREWALL-DENY";
    }
    return "UNKNOWN";
}

bool hook_self_test() {
    security::firewall::Engine engine;
    engine.reset();
    const security::firewall::AllowRuleV4 dns{
        42, true, security::firewall::Direction::egress, security::firewall::Protocol::udp,
        security::firewall::ipv4(192, 168, 1, 0), 24,
        security::firewall::ipv4(1, 1, 1, 1), 32,
        1024, 65535, 53, 53
    };
    if (!engine.add_allow_rule(dns)) return false;

    Hook hook(engine);
    uint8_t frame[64]{};
    const size_t n = make_udp(frame, sizeof(frame), security::firewall::ipv4(192, 168, 1, 50), security::firewall::ipv4(1, 1, 1, 1), 53000, 53);
    if (n == 0) return false;

    auto d = hook.inspect(security::firewall::Direction::egress, frame, n);
    if (d.verdict != security::firewall::Verdict::allow || d.reason != HookReason::firewall_allow || d.rule_id != 42) return false;

    d = hook.inspect(security::firewall::Direction::ingress, frame, n);
    if (d.verdict != security::firewall::Verdict::deny || d.reason != HookReason::firewall_deny) return false;

    frame[12] = 0x08;
    frame[13] = 0x06;
    d = hook.inspect(security::firewall::Direction::egress, frame, n);
    if (d.verdict != security::firewall::Verdict::deny || d.reason != HookReason::parse_reject || d.parse_status != ParseStatus::unsupported_ethertype) return false;

    for (size_t i = 0; i < hook_audit_capacity + 3; ++i) {
        hook.inspect(security::firewall::Direction::egress, frame, n);
    }
    if (hook.audit_count() != hook_audit_capacity) return false;
    HookAuditEvent first{};
    HookAuditEvent last{};
    if (!hook.audit_get_oldest(0, first)) return false;
    if (!hook.audit_get_oldest(hook_audit_capacity - 1, last)) return false;
    if (last.sequence <= first.sequence) return false;
    return true;
}

}
