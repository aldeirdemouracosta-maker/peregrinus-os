#pragma once
#include <stddef.h>
#include <stdint.h>

namespace peregrinus::net::e1000 {

struct __attribute__((packed, aligned(16))) RxDescriptor {
    uint64_t buffer_addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
};

struct __attribute__((packed, aligned(16))) TxDescriptor {
    uint64_t buffer_addr;
    uint16_t length;
    uint8_t cso;
    uint8_t command;
    uint8_t status;
    uint8_t css;
    uint16_t special;
};

static_assert(sizeof(RxDescriptor) == 16);
static_assert(sizeof(TxDescriptor) == 16);

inline constexpr uint8_t rx_status_dd = 1u << 0;
inline constexpr uint8_t rx_status_eop = 1u << 1;
inline constexpr uint8_t tx_status_dd = 1u << 0;
inline constexpr uint8_t tx_cmd_eop = 1u << 0;
inline constexpr uint8_t tx_cmd_ifcs = 1u << 1;
inline constexpr uint8_t tx_cmd_rs = 1u << 3;
inline constexpr size_t ring_count = 8;
inline constexpr size_t buffer_bytes = 2048;

bool rx_complete(const RxDescriptor& d);
bool rx_frame_valid(const RxDescriptor& d);
size_t ring_next(size_t index);
bool model_self_test();

}
