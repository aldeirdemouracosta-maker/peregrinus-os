#pragma once
#include <stddef.h>
#include <stdint.h>
#include "ethernet_ipv4.hpp"

namespace peregrinus::net {

enum class HookReason : uint8_t {
    parse_reject = 0,
    firewall_allow,
    firewall_deny,
};

struct HookDecision {
    security::firewall::Verdict verdict;
    HookReason reason;
    ParseStatus parse_status;
    uint32_t rule_id;
};

struct HookAuditEvent {
    uint64_t sequence;
    security::firewall::Direction direction;
    size_t frame_size;
    uint16_t ether_type;
    HookDecision decision;
};

inline constexpr size_t hook_audit_capacity = 64;

class Hook {
public:
    explicit Hook(security::firewall::Engine& engine) : engine_(&engine) {}
    HookDecision inspect(security::firewall::Direction direction, const uint8_t* frame, size_t frame_size);
    size_t audit_count() const { return audit_count_; }
    bool audit_get_oldest(size_t index, HookAuditEvent& out) const;

private:
    security::firewall::Engine* engine_;
    HookAuditEvent audit_[hook_audit_capacity]{};
    size_t audit_head_ = 0;
    size_t audit_count_ = 0;
    uint64_t next_sequence_ = 1;
    void audit(security::firewall::Direction direction, size_t frame_size, uint16_t ether_type, const HookDecision& decision);
};

const char* hook_reason_name(HookReason reason);
bool hook_self_test();

}
