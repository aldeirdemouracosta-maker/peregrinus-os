// ACPI root-table discovery: XSDT on ACPI 2.0+, RSDT fallback on ACPI 1.0 firmware.
// Tables live in one host buffer; "physical" addresses are offsets from its start, mapped
// through the HHDM offset exactly as the kernel does.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "acpi/acpi.hpp"
#include "../third_party/limine/limine_min.h"

namespace peregrinus::serial { void write(const char*) {} void writeln(const char*) {} }
namespace peregrinus::format { void dec64(uint64_t, char o[24]) { o[0] = 0; } void hex64(uint64_t, char o[19]) { o[0] = 0; } }

alignas(16) static uint8_t mem[4096];
static void fix_checksum(uint8_t* t, uint32_t len, uint32_t at) { t[at] = 0; uint8_t s = 0; for (uint32_t i = 0; i < len; ++i) s = uint8_t(s + t[i]); t[at] = uint8_t(-s); }
static void sdt(uint32_t off, const char* sig, uint32_t len) { std::memcpy(mem + off, sig, 4); std::memcpy(mem + off + 4, &len, 4); }

static void build_madt(uint32_t off) {
    const uint32_t len = 44 + 8 + 12;          // header+lapic/flags, one LAPIC, one IOAPIC
    sdt(off, "APIC", len);
    uint32_t lapic = 0xFEE00000u; std::memcpy(mem + off + 36, &lapic, 4);
    uint8_t* e = mem + off + 44; e[0] = 0; e[1] = 8; e[4] = 1;              // enabled LAPIC
    e += 8; e[0] = 1; e[1] = 12;                                            // IOAPIC
    fix_checksum(mem + off, len, 9);
}

static bool run(bool acpi2) {
    std::memset(mem, 0, sizeof mem);
    build_madt(1024);
    uint8_t* rsdp = mem;  // RSDP at offset 0
    std::memcpy(rsdp, "RSD PTR ", 8);
    if (acpi2) {
        rsdp[15] = 2; sdt(512, "XSDT", 36 + 8); uint64_t e = 1024; std::memcpy(mem + 512 + 36, &e, 8); fix_checksum(mem + 512, 44, 9);
        uint64_t x = 512; std::memcpy(rsdp + 24, &x, 8);
    } else {
        rsdp[15] = 0; sdt(512, "RSDT", 36 + 4); uint32_t e = 1024; std::memcpy(mem + 512 + 36, &e, 4); fix_checksum(mem + 512, 40, 9);
        uint32_t r = 512; std::memcpy(rsdp + 16, &r, 4);
    }
    limine_rsdp_response rsp{0, rsdp};
    limine_hhdm_response hh{}; hh.offset = reinterpret_cast<uint64_t>(mem);
    peregrinus::acpi::init(&rsp, &hh);
    return peregrinus::acpi::madt_present() && peregrinus::acpi::madt().local_apics == 1 && peregrinus::acpi::madt().io_apics == 1;
}

int main() {
    if (!run(true)) { std::puts("FAIL: MADT via XSDT"); return 1; }
    if (!run(false)) { std::puts("FAIL: MADT via RSDT (ACPI 1.0)"); return 2; }
    return 0;
}
