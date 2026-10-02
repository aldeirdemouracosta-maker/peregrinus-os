#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../security/firewall.hpp"
#include "ethernet_ipv4.hpp"
#include "stateful_guard.hpp"
#include "burst_guard.hpp"

namespace peregrinus::net {

enum class DatapathReason:uint8_t{parse_reject=0,explicit_allow,stateful_reply,default_deny,burst_guard};
struct DatapathDecision{security::firewall::Verdict verdict;DatapathReason reason;ParseStatus parse_status;uint32_t rule_id;};
struct DatapathAudit{uint64_t sequence;security::firewall::Direction direction;size_t frame_size;DatapathDecision decision;};
inline constexpr size_t datapath_audit_capacity=128;

class Datapath {
public:
    void reset();
    bool add_allow_rule(const security::firewall::AllowRuleV4& rule){return engine_.add_allow_rule(rule);}
    DatapathDecision inspect(security::firewall::Direction direction,const uint8_t* frame,size_t bytes);
    size_t audit_count()const{return audit_count_;}
    bool audit_get_oldest(size_t index,DatapathAudit& out)const;
    uint64_t allowed()const{return allowed_;}
    uint64_t denied()const{return denied_;}
    const stateful::Guard& stateful_guard()const{return stateful_;}
    const burst::Guard& burst_guard()const{return burst_;}
private:
    security::firewall::Engine engine_{};stateful::Guard stateful_{};burst::Guard burst_{};
    DatapathAudit audit_[datapath_audit_capacity]{};size_t audit_head_=0,audit_count_=0;uint64_t sequence_=0,allowed_=0,denied_=0;
    void record(security::firewall::Direction,size_t,const DatapathDecision&);
};
const char* datapath_reason_name(DatapathReason reason);
bool datapath_self_test();
}
