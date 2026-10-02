#include "e1000.hpp"
#include "../arch/x86_64/cpu.hpp"
#include "../config/features.hpp"
#include "../mm/memory.hpp"
#include "../mm/mmio.hpp"
#include "../console/serial.hpp"

namespace peregrinus::net::e1000 {
namespace {
constexpr uint32_t REG_STATUS = 0x00008;
constexpr uint32_t REG_ICR    = 0x000c0;
constexpr uint32_t REG_IMC    = 0x000d8;
constexpr uint32_t REG_RCTL   = 0x00100;
constexpr uint32_t REG_TCTL   = 0x00400;
constexpr uint32_t REG_RDBAL  = 0x02800;
constexpr uint32_t REG_RDBAH  = 0x02804;
constexpr uint32_t REG_RDLEN  = 0x02808;
constexpr uint32_t REG_RDH    = 0x02810;
constexpr uint32_t REG_RDT    = 0x02818;
constexpr uint32_t REG_TDBAL  = 0x03800;
constexpr uint32_t REG_TDBAH  = 0x03804;
constexpr uint32_t REG_TDLEN  = 0x03808;
constexpr uint32_t REG_TDH    = 0x03810;
constexpr uint32_t REG_TDT    = 0x03818;
constexpr uint32_t REG_RAL0   = 0x05400;
constexpr uint32_t REG_RAH0   = 0x05404;
constexpr uint32_t RCTL_EN    = 1u << 1;
constexpr uint32_t RCTL_BAM   = 1u << 15;
constexpr uint32_t RCTL_SECRC = 1u << 26;
constexpr uint32_t TCTL_EN    = 1u << 1;
constexpr uint32_t TCTL_PSP   = 1u << 3;
constexpr uint32_t TCTL_CT    = 0x10u << 4;
constexpr uint32_t TCTL_COLD  = 0x40u << 12;
constexpr uint32_t RAH_AV     = 1u << 31;
constexpr size_t MMIO_BYTES   = 0x20000;

Driver g_driver{};

void zero_page(void* p) {
    auto* b = static_cast<uint8_t*>(p);
    for (size_t i = 0; i < 4096; ++i) b[i] = 0;
}

const pci::Device* find_qemu_e1000() {
    const auto* ds = pci::devices();
    for (uint32_t i = 0; i < pci::device_count(); ++i) {
        const auto& d = ds[i];
        if (d.class_code == 0x02 && d.subclass == 0x00 && d.vendor == 0x8086 && d.device == 0x100e) return &d;
    }
    return nullptr;
}

bool mac_nonzero(const MacAddress& m) {
    uint8_t x = 0;
    for (uint8_t b : m.bytes) x |= b;
    return x != 0 && (m.bytes[0] & 1u) == 0;
}
}

uint32_t Driver::reg_read(uint32_t off) const {
    if (!mmio_ || off + sizeof(uint32_t) > MMIO_BYTES) return 0xffffffffu;
    return *reinterpret_cast<volatile uint32_t*>(const_cast<volatile uint8_t*>(mmio_) + off);
}

void Driver::reg_write(uint32_t off, uint32_t value) {
    if (!mmio_ || off + sizeof(uint32_t) > MMIO_BYTES) return;
    *reinterpret_cast<volatile uint32_t*>(mmio_ + off) = value;
    (void)reg_read(off);
}

InitStatus Driver::init_qemu_sandbox() {
    ready_ = false;
    stats_ = {};
    if (!features::nic_driver_live) return InitStatus::disabled;
    if (!cpu::qemu_qualification_environment()) return InitStatus::environment_rejected;
    const pci::Device* dev = find_qemu_e1000();
    if (!dev) return InitStatus::device_not_found;
    const auto bar = pci::bar_info(*dev, 0);
    if (bar.io || bar.address == 0) return InitStatus::bar_invalid;
    if (!pci::enable_memory_busmaster(*dev)) return InitStatus::pci_enable_failed;
    mmio_ = static_cast<volatile uint8_t*>(mmio::map(bar.address, MMIO_BYTES));
    if (!mmio_) return InitStatus::mmio_failed;

    auto alloc_dma_page = [](uint64_t& phys, uint8_t*& virt) -> bool {
        phys = reinterpret_cast<uint64_t>(memory::alloc_dma32_page());
        if (!phys) return false;
        virt = static_cast<uint8_t*>(memory::phys_to_hhdm(phys));
        if (!virt) return false;
        zero_page(virt);
        return true;
    };

    uint8_t* ring_page = nullptr;
    if (!alloc_dma_page(rx_ring_phys_, ring_page)) return InitStatus::dma_failed;
    rx_ring_ = reinterpret_cast<RxDescriptor*>(ring_page);
    if (!alloc_dma_page(tx_ring_phys_, ring_page)) return InitStatus::dma_failed;
    tx_ring_ = reinterpret_cast<TxDescriptor*>(ring_page);
    for (size_t i = 0; i < ring_count; ++i) {
        if (!alloc_dma_page(rx_buffer_phys_[i], rx_buffers_[i])) return InitStatus::dma_failed;
        if (!alloc_dma_page(tx_buffer_phys_[i], tx_buffers_[i])) return InitStatus::dma_failed;
        rx_ring_[i] = {};
        rx_ring_[i].buffer_addr = rx_buffer_phys_[i];
        tx_ring_[i] = {};
        tx_ring_[i].buffer_addr = tx_buffer_phys_[i];
        tx_ring_[i].status = tx_status_dd;
    }

    const uint32_t ral = reg_read(REG_RAL0);
    const uint32_t rah = reg_read(REG_RAH0);
    if ((rah & RAH_AV) == 0) return InitStatus::mac_invalid;
    mac_.bytes[0] = static_cast<uint8_t>(ral);
    mac_.bytes[1] = static_cast<uint8_t>(ral >> 8);
    mac_.bytes[2] = static_cast<uint8_t>(ral >> 16);
    mac_.bytes[3] = static_cast<uint8_t>(ral >> 24);
    mac_.bytes[4] = static_cast<uint8_t>(rah);
    mac_.bytes[5] = static_cast<uint8_t>(rah >> 8);
    if (!mac_nonzero(mac_)) return InitStatus::mac_invalid;

    reg_write(REG_IMC, 0xffffffffu);
    (void)reg_read(REG_ICR);
    reg_write(REG_RCTL, 0);
    reg_write(REG_TCTL, 0);

    reg_write(REG_RDBAL, static_cast<uint32_t>(rx_ring_phys_));
    reg_write(REG_RDBAH, static_cast<uint32_t>(rx_ring_phys_ >> 32));
    reg_write(REG_RDLEN, static_cast<uint32_t>(ring_count * sizeof(RxDescriptor)));
    reg_write(REG_RDH, 0);
    reg_write(REG_RDT, static_cast<uint32_t>(ring_count - 1));

    reg_write(REG_TDBAL, static_cast<uint32_t>(tx_ring_phys_));
    reg_write(REG_TDBAH, static_cast<uint32_t>(tx_ring_phys_ >> 32));
    reg_write(REG_TDLEN, static_cast<uint32_t>(ring_count * sizeof(TxDescriptor)));
    reg_write(REG_TDH, 0);
    reg_write(REG_TDT, 0);

    asm volatile("mfence" ::: "memory");
    reg_write(REG_RCTL, RCTL_EN | RCTL_BAM | RCTL_SECRC);
    reg_write(REG_TCTL, TCTL_EN | TCTL_PSP | TCTL_CT | TCTL_COLD);
    rx_next_ = 0;
    tx_next_ = 0;
    ready_ = true;
    serial::writeln("e1000 sandbox: RX/TX polling rings READY; interrupts disabled");
    return InitStatus::ready;
}

size_t Driver::poll_rx(RxConsumer consumer, void* context, size_t budget) {
    if (!ready_ || !consumer) return 0;
    size_t processed = 0;
    while (processed < budget) {
        RxDescriptor& d = rx_ring_[rx_next_];
        asm volatile("lfence" ::: "memory");
        if (!rx_complete(d)) break;
        if (rx_frame_valid(d)) {
            ++stats_.rx_frames;
            if (consumer(rx_buffers_[rx_next_], d.length, context)) ++stats_.rx_allowed_policy;
            else ++stats_.rx_dropped_policy;
        } else {
            ++stats_.rx_invalid;
        }
        d.status = 0;
        d.errors = 0;
        d.length = 0;
        asm volatile("mfence" ::: "memory");
        reg_write(REG_RDT, static_cast<uint32_t>(rx_next_));
        rx_next_ = ring_next(rx_next_);
        ++processed;
    }
    return processed;
}

bool Driver::transmit(const uint8_t* frame, size_t bytes) {
    if (!ready_ || !frame || bytes < 14 || bytes > 1514) return false;
    TxDescriptor& d = tx_ring_[tx_next_];
    if (!tx_wait_done(d, 100000)) { ++stats_.tx_failures; return false; }
    for (size_t i = 0; i < bytes; ++i) tx_buffers_[tx_next_][i] = frame[i];
    d.length = static_cast<uint16_t>(bytes);
    d.cso = 0;
    d.command = tx_cmd_eop | tx_cmd_ifcs | tx_cmd_rs;
    d.status = 0;
    d.css = 0;
    d.special = 0;
    asm volatile("mfence" ::: "memory");
    const size_t next = ring_next(tx_next_);
    reg_write(REG_TDT, static_cast<uint32_t>(next));
    if (!tx_wait_done(d, 500000)) { ++stats_.tx_failures; return false; }
    tx_next_ = next;
    ++stats_.tx_frames;
    return true;
}

uint32_t Driver::mmio_status() const { return reg_read(REG_STATUS); }
Driver& driver() { return g_driver; }

const char* init_status_name(InitStatus s) {
    switch (s) {
        case InitStatus::disabled: return "DISABLED";
        case InitStatus::environment_rejected: return "ENVIRONMENT-REJECTED";
        case InitStatus::device_not_found: return "DEVICE-NOT-FOUND";
        case InitStatus::bar_invalid: return "BAR-INVALID";
        case InitStatus::pci_enable_failed: return "PCI-ENABLE-FAILED";
        case InitStatus::mmio_failed: return "MMIO-FAILED";
        case InitStatus::dma_failed: return "DMA-FAILED";
        case InitStatus::mac_invalid: return "MAC-INVALID";
        case InitStatus::ready: return "READY";
    }
    return "UNKNOWN";
}

}
