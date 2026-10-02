#pragma once
#include <stddef.h>
#include <stdint.h>

namespace peregrinus::security::firewall {

enum class Direction : uint8_t { ingress = 0, egress = 1 };
enum class Protocol : uint8_t { any = 0, icmp = 1, tcp = 6, udp = 17 };
enum class Verdict : uint8_t { deny = 0, allow = 1 };
enum class Reason : uint8_t {
    invalid_packet = 0,
    fragment_unsupported,
    allow_rule,
    default_deny,
};

struct PacketV4 {
    Direction direction;
    Protocol protocol;
    uint32_t src;
    uint32_t dst;
    uint16_t src_port;
    uint16_t dst_port;
    bool fragmented;
    uint8_t tcp_flags;
    uint8_t icmp_type;
    uint8_t icmp_code;
};

struct AllowRuleV4 {
    uint32_t id;
    bool enabled;
    Direction direction;
    Protocol protocol;
    uint32_t src_network;
    uint8_t src_prefix;
    uint32_t dst_network;
    uint8_t dst_prefix;
    uint16_t src_port_first;
    uint16_t src_port_last;
    uint16_t dst_port_first;
    uint16_t dst_port_last;
};

struct Decision {
    Verdict verdict;
    Reason reason;
    uint32_t rule_id;
};

struct AuditEvent {
    uint64_t sequence;
    Decision decision;
    PacketV4 packet;
};

inline constexpr size_t max_rules = 16;
inline constexpr size_t audit_capacity = 64;

class Engine {
public:
    void reset();
    bool add_allow_rule(const AllowRuleV4& rule);
    Decision evaluate(const PacketV4& packet);
    size_t rule_count() const { return rule_count_; }
    size_t audit_count() const { return audit_count_; }
    bool audit_get_oldest(size_t index, AuditEvent& out) const;

private:
    AllowRuleV4 rules_[max_rules]{};
    size_t rule_count_ = 0;
    AuditEvent audit_[audit_capacity]{};
    size_t audit_head_ = 0;
    size_t audit_count_ = 0;
    uint64_t next_sequence_ = 1;

    void audit(const PacketV4& packet, const Decision& decision);
};

uint32_t ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
const char* verdict_name(Verdict verdict);
const char* reason_name(Reason reason);
bool self_test();

}
