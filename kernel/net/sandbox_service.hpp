#pragma once
#include <stddef.h>
#include <stdint.h>
#include "datapath.hpp"
#include "e1000.hpp"

namespace peregrinus::net::sandbox {

struct Stats {
    uint64_t arp_requests;
    uint64_t arp_replies_sent;
    uint64_t ipv4_allowed;
    uint64_t ipv4_denied;
    uint64_t icmp_replies_sent;
    uint64_t tx_failures;
};

class Service {
public:
    bool init();
    size_t poll(size_t budget);
    const Stats& stats() const { return stats_; }
    const Datapath& datapath() const { return datapath_; }
    uint32_t local_ip() const { return local_ip_; }
private:
    static bool consume(const uint8_t* frame,size_t bytes,void* context);
    bool on_frame(const uint8_t* frame,size_t bytes);
    Datapath datapath_{};
    e1000::Driver* nic_=nullptr;
    e1000::MacAddress mac_{};
    uint32_t local_ip_=0x0a00020fu; // 10.0.2.15, QEMU user-network qualification profile.
    Stats stats_{};
};

Service& service();

}
