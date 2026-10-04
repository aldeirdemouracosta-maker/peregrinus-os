// e1000 driver against an emulated register file (host memory + a thread playing the NIC).
// Checks the init order (reset before bus mastering, rings programmed before DMA is granted)
// and that a TX completion timeout disables the NIC instead of desynchronising the ring.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include "arch/x86_64/cpu.hpp"
#include "pci/pci.hpp"
#include "mm/memory.hpp"
#include "mm/mmio.hpp"
#include "console/serial.hpp"
#include "net/e1000.hpp"

namespace {
alignas(4096) uint8_t g_bar[0x20000];
alignas(4096) uint8_t g_pages[24][4096];
unsigned g_next = 0;
peregrinus::pci::Device g_nic{0, 3, 0, 0x02, 0x00, 0x00, 0, 0x8086, 0x100e};
volatile uint32_t* reg(uint32_t off) { return reinterpret_cast<volatile uint32_t*>(g_bar + off); }
std::atomic<bool> g_stop{false}, g_bm{false}, g_order_ok{false}, g_reset_seen{false};
// The emulated NIC completes resets but never completes a transmission (TX hang).
void nic() {
    while (!g_stop) {
        if (*reg(0x0) & (1u << 26)) { g_reset_seen = true; *reg(0x0) &= ~(1u << 26); }
        std::this_thread::yield();
    }
}
}

namespace peregrinus::cpu { bool qemu_qualification_environment() { return true; } }
namespace peregrinus::pci {
const Device* devices() { return &g_nic; }
uint32_t device_count() { return 1; }
BarInfo bar_info(const Device&, uint8_t) { return {reinterpret_cast<uint64_t>(g_bar), false, false}; }
bool enable_memory_space(const Device&) { return true; }
bool enable_bus_master(const Device&) {
    // DMA may only be granted after a completed reset and once both rings are programmed.
    g_order_ok = g_reset_seen && (*reg(0x0) & (1u << 26)) == 0 && *reg(0x2800) != 0 && *reg(0x3800) != 0;
    g_bm = true; return true;
}
void disable_bus_master(const Device&) { g_bm = false; }
}
namespace peregrinus::memory {
void* alloc_dma32_page() { return g_next < 24 ? g_pages[g_next++] : nullptr; }
void* phys_to_hhdm(uint64_t phys) { return reinterpret_cast<void*>(phys); }
}
namespace peregrinus::mmio { void* map(uint64_t phys, size_t, Cache) { return reinterpret_cast<void*>(phys); } }
namespace peregrinus::serial { void write(const char*) {} void writeln(const char*) {} }

int main() {
    using namespace peregrinus::net::e1000;
    *reg(0x5400) = 0x12005452u;      // RAL: 52:54:00:12
    *reg(0x5404) = 0x80005634u;      // RAH: 34:56 + AV
    *reg(0x0) = 1u << 2;             // pretend firmware left something in CTRL
    std::thread t(nic);
    auto finish = [&](int rc, const char* msg) { if (msg) std::puts(msg); g_stop = true; t.join(); return rc; };
    Driver d;
    if (d.init_qemu_sandbox() != InitStatus::ready) return finish(1, "FAIL: init");
    if (!g_order_ok) return finish(2, "FAIL: bus mastering granted before reset/ring setup");
    uint8_t frame[60]{}; std::memset(frame, 0xff, 6);
    if (d.transmit(frame, sizeof frame)) return finish(3, "FAIL: transmit succeeded although the NIC never completed it");
    if (d.ready()) return finish(4, "FAIL: driver still ready after TX timeout");
    if (g_bm) return finish(5, "FAIL: bus mastering still enabled after fail-closed");
    if (d.transmit(frame, sizeof frame)) return finish(6, "FAIL: transmit accepted on a disabled NIC");
    return finish(0, nullptr);
}
