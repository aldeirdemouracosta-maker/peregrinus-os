#include "e1000_model.hpp"

namespace peregrinus::net::e1000 {

bool rx_complete(const RxDescriptor& d) {
    return (d.status & rx_status_dd) != 0;
}

bool rx_frame_valid(const RxDescriptor& d) {
    return rx_complete(d) && (d.status & rx_status_eop) != 0 && d.errors == 0 &&
           d.length >= 14 && d.length <= buffer_bytes;
}

size_t ring_next(size_t index) {
    return (index + 1u) % ring_count;
}

bool tx_wait_done(const TxDescriptor& d, uint32_t spin_budget) {
    const volatile uint8_t* status = &d.status;
    while ((*status & tx_status_dd) == 0) {
        if (spin_budget == 0) return false;
        --spin_budget;
        __builtin_ia32_pause();
    }
    return true;
}

bool model_self_test() {
    if (ring_count * sizeof(RxDescriptor) != 128) return false;
    if (ring_count * sizeof(TxDescriptor) != 128) return false;
    if (ring_next(ring_count - 1) != 0 || ring_next(2) != 3) return false;
    RxDescriptor r{};
    r.length = 64;
    r.status = rx_status_dd | rx_status_eop;
    if (!rx_complete(r) || !rx_frame_valid(r)) return false;
    r.errors = 1;
    if (rx_frame_valid(r)) return false;
    r.errors = 0;
    r.length = buffer_bytes + 1;
    if (rx_frame_valid(r)) return false;
    TxDescriptor t{};
    if (tx_wait_done(t, 0) || tx_wait_done(t, 16)) return false;
    t.status = tx_status_dd;
    if (!tx_wait_done(t, 0) || !tx_wait_done(t, 16)) return false;
    return true;
}

}
