#pragma once
#include <stddef.h>
#include <stdint.h>
#include "e1000_model.hpp"
#include "../pci/pci.hpp"

namespace peregrinus::net::e1000 {

struct MacAddress { uint8_t bytes[6]; };
struct Stats {
    uint64_t rx_frames;
    uint64_t rx_invalid;
    uint64_t rx_dropped_policy;
    uint64_t rx_allowed_policy;
    uint64_t tx_frames;
    uint64_t tx_failures;
};

enum class InitStatus : uint8_t {
    disabled = 0,
    environment_rejected,
    device_not_found,
    bar_invalid,
    pci_enable_failed,
    mmio_failed,
    dma_failed,
    mac_invalid,
    reset_failed,
    bus_master_failed,
    ready,
};

using RxConsumer = bool (*)(const uint8_t* frame, size_t bytes, void* context);

class Driver {
public:
    InitStatus init_qemu_sandbox();
    bool ready() const { return ready_; }
    MacAddress mac() const { return mac_; }
    size_t poll_rx(RxConsumer consumer, void* context, size_t budget);
    bool transmit(const uint8_t* frame, size_t bytes);
    const Stats& stats() const { return stats_; }
    uint32_t mmio_status() const;
private:
    volatile uint8_t* mmio_ = nullptr;
    RxDescriptor* rx_ring_ = nullptr;
    TxDescriptor* tx_ring_ = nullptr;
    uint64_t rx_ring_phys_ = 0;
    uint64_t tx_ring_phys_ = 0;
    uint8_t* rx_buffers_[ring_count]{};
    uint8_t* tx_buffers_[ring_count]{};
    uint64_t rx_buffer_phys_[ring_count]{};
    uint64_t tx_buffer_phys_[ring_count]{};
    size_t rx_next_ = 0;
    size_t tx_next_ = 0;
    MacAddress mac_{};
    Stats stats_{};
    bool ready_ = false;
    const pci::Device* dev_ = nullptr;
    void fail_closed(const char* why);
    uint32_t reg_read(uint32_t off) const;
    void reg_write(uint32_t off, uint32_t value);
};

Driver& driver();
const char* init_status_name(InitStatus status);

}
