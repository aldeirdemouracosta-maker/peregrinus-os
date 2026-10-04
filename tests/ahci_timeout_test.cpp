// Regression test for the AHCI wait loops: a command that never completes, or that ends with a
// task-file error, must be reported as FAILED and must poison the controller (fail-closed).
// The HBA is emulated in host memory; a helper thread plays the device.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include "pci/pci.hpp"
#include "mm/memory.hpp"
#include "mm/mmio.hpp"
#include "console/serial.hpp"
#include "console/format.hpp"
#include "storage/ahci.hpp"
#include "runtime/spin.hpp"

namespace {
alignas(4096) uint8_t g_hba[0x1100];
alignas(4096) uint8_t g_pages[8][4096];
unsigned g_next_page = 0;
peregrinus::pci::Device g_ctl{0, 1, 0, 0x01, 0x06, 0x01, 0, 0x8086, 0x2922};
volatile uint32_t* reg(uint32_t off) { return reinterpret_cast<volatile uint32_t*>(g_hba + off); }
constexpr uint32_t P0 = 0x100, PxIS = P0 + 0x10, PxSIG = P0 + 0x24, PxSSTS = P0 + 0x28, PxCI = P0 + 0x38;
enum class Mode { complete, hang, tf_error };
std::atomic<Mode> g_mode{Mode::complete};
std::atomic<bool> g_stop{false};
void device() {
    while (!g_stop.load()) {
        const uint32_t ci = *reg(PxCI);
        if (ci) {
            if (g_mode == Mode::complete) { std::memset(g_pages[3], 0x5A, 512); *reg(PxCI) = 0; }
            else if (g_mode == Mode::tf_error) { *reg(PxIS) = 1u << 30; }
        }
        std::this_thread::yield();
    }
}
}

namespace peregrinus::pci {
const Device* devices() { return &g_ctl; }
uint32_t device_count() { return 1; }
BarInfo bar_info(const Device&, uint8_t) { return {reinterpret_cast<uint64_t>(g_hba), false, true}; }
bool memory_space_enabled(const Device&) { return true; }
bool enable_memory_busmaster(const Device&) { return true; }
}
namespace peregrinus::memory {
void* alloc_dma32_page() { return g_next_page < 8 ? g_pages[g_next_page++] : nullptr; }
void* phys_to_hhdm(uint64_t phys) { return reinterpret_cast<void*>(phys); }
}
namespace peregrinus::mmio { void* map(uint64_t phys, size_t, Cache) { return reinterpret_cast<void*>(phys); } }
namespace peregrinus::serial { void write(const char*) {} void writeln(const char*) {} }
namespace peregrinus::format { void dec64(uint64_t, char o[24]) { o[0] = 0; } void hex64(uint64_t, char o[19]) { o[0] = 0; } }

int main() {
    using namespace peregrinus;
    // spin::until must expire after exactly budget+1 polls and never report expiry as success.
    for (uint32_t budget : {0u, 1u, 5u, 1000u}) {
        uint32_t polls = 0;
        if (spin::until([&] { ++polls; return false; }, budget) || polls != budget + 1) { std::puts("FAIL: spin budget"); return 10; }
    }
    { uint32_t polls = 0; if (!spin::until([&] { return ++polls == 3; }, 5) || polls != 3) { std::puts("FAIL: spin success"); return 11; } }
    *reg(0x00) = 0;                 // CAP: one command slot
    *reg(0x0C) = 1;                 // PI: port 0
    *reg(PxSIG) = 0x00000101u;      // ATA
    *reg(PxSSTS) = 0x113;           // DET=3, IPM=1
    ahci::inspect_readonly();
    std::thread dev(device);
    uint8_t out[512]{};

    g_mode = Mode::complete;
    if (!ahci::identify_device_guarded(0, out) || out[0] != 0x5A) { std::puts("FAIL: completed command not reported"); g_stop = true; dev.join(); return 1; }

    g_mode = Mode::hang;
    if (ahci::identify_device_guarded(0, out)) { std::puts("FAIL: timed-out command reported as success"); g_stop = true; dev.join(); return 2; }
    if (!ahci::result().poisoned) { std::puts("FAIL: controller not poisoned after timeout"); g_stop = true; dev.join(); return 3; }

    g_mode = Mode::complete;
    *reg(PxCI) = 0;
    if (ahci::identify_device_guarded(0, out)) { std::puts("FAIL: poisoned controller accepted a new command"); g_stop = true; dev.join(); return 4; }
    const uint8_t sector[512]{};
    if (ahci::write_recovery_journal_sector_guarded(0, 32770, 131071, 32768, 49151, sector)) { std::puts("FAIL: poisoned controller accepted a write"); g_stop = true; dev.join(); return 5; }

    // Task-file error path on a fresh controller.
    g_stop = true; dev.join(); g_stop = false; g_next_page = 0; *reg(PxIS) = 0;
    ahci::inspect_readonly();
    std::thread dev2(device);
    g_mode = Mode::tf_error;
    if (ahci::write_recovery_journal_sector_guarded(0, 32770, 131071, 32768, 49151, sector)) { std::puts("FAIL: task-file error reported as success"); g_stop = true; dev2.join(); return 6; }
    if (!ahci::result().poisoned) { std::puts("FAIL: controller not poisoned after task-file error"); g_stop = true; dev2.join(); return 7; }
    g_stop = true; dev2.join();
    return 0;
}
