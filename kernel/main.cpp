#include <stdint.h>
#include "console/serial.hpp"
#include "console/framebuffer.hpp"
#include "console/format.hpp"
#include "arch/x86_64/cpu.hpp"
#include "arch/x86_64/gdt.hpp"
#include "arch/x86_64/idt.hpp"
#include "mm/memory.hpp"
#include "mm/paging.hpp"
#include "mm/mmio.hpp"
#include "pci/pci.hpp"
#include "boot/limine_requests.hpp"
#include "panic/panic.hpp"
#include "acpi/acpi.hpp"
#include "hw/manifest.hpp"
#include "storage/ahci.hpp"
#include "storage/gpt.hpp"
#include "storage/disk_probe.hpp"
#include "storage/storage.hpp"
#include "storage/volume.hpp"
#include "storage/identify.hpp"
#include "storage/recovery_anchor.hpp"
#include "storage/recovery_probe.hpp"
#include "storage/boot_policy.hpp"
#include "security/sha256.hpp"
#include "security/integrity.hpp"
#include "security/quarantine.hpp"
#include "security/watchdog.hpp"
#include "security/guard_policy.hpp"
#include "security/root_trust.hpp"
#include "security/itco_watchdog.hpp"
#include "security/firewall.hpp"
#include "net/ethernet_ipv4.hpp"
#include "net/hook.hpp"
#include "net/nic_probe.hpp"
#include "net/sandbox_service.hpp"
#include "storage/recovery_journal.hpp"
#include "storage/recovery_journal_probe.hpp"
#include "config/features.hpp"

static void halt_forever(){for(;;)asm volatile("hlt");}
static void require_watchdog(peregrinus::security::watchdog::Stage s){if(!peregrinus::security::watchdog::checkpoint(s))peregrinus::panic::stop("Peregrinus boot watchdog sequence violation");}

extern "C" void kmain(){
    using namespace peregrinus;
    security::watchdog::reset();require_watchdog(security::watchdog::Stage::boot_entry);
    serial::init();serial::writeln("");serial::writeln("====================================");serial::writeln(" Peregrinus OS — Purgatorio 0.1 Admission Gate");serial::writeln("====================================");
    if(!LIMINE_BASE_REVISION_SUPPORTED(boot::base_revision))panic::stop("Limine base revision unsupported");
    if(boot::bootloader_info_request.response){serial::write("Bootloader: ");serial::write(boot::bootloader_info_request.response->name);serial::write(" ");serial::writeln(boot::bootloader_info_request.response->version);}
    cpu::report();gdt::init();idt::init();require_watchdog(security::watchdog::Stage::cpu_ready);

    memory::init_from_limine(boot::memmap_request.response);
    memory::set_hhdm_offset(boot::hhdm_request.response?boot::hhdm_request.response->offset:0);
    memory::init_page_allocator(boot::memmap_request.response);
    if(!paging::init())serial::writeln("WARNING: Peregrinus page mapper unavailable; MMIO will remain blocked");
    if(!mmio::init())serial::writeln("WARNING: dedicated MMIO mapper unavailable");
    require_watchdog(security::watchdog::Stage::memory_ready);

    serial::write("Boot slot: ");serial::writeln(security::root_trust::slot_name(security::root_trust::running_slot()));
    {char g[24];format::dec64(security::root_trust::build_generation,g);serial::write("Build generation: ");serial::writeln(g);}
    {char e[24];format::dec64(security::root_trust::security_epoch,e);serial::write("Security epoch: ");serial::writeln(e);}
    const auto epoch_guard=security::root_trust::evaluate_epoch(security::root_trust::minimum_security_epoch);
    if(!epoch_guard.accepted)panic::stop("Peregrinus security epoch below minimum");
    const auto integrity=security::integrity::verify_kernel_text();
    serial::write("Kernel integrity manifest: ");serial::writeln(integrity.manifest_valid?"VALID":"INVALID");
    serial::write("Kernel .text SHA-256: ");serial::writeln(integrity.trusted()?"PASS":"FAIL");
    if(!integrity.trusted())panic::stop("Peregrinus Guard kernel integrity failure");
    require_watchdog(security::watchdog::Stage::integrity_ready);

    if(boot::framebuffer_request.response&&boot::framebuffer_request.response->framebuffer_count){auto* fb=boot::framebuffer_request.response->framebuffers[0];if(framebuffer::init(fb)){framebuffer::status_bars();serial::writeln("Framebuffer: initialized");}else serial::writeln("Framebuffer: unsupported mode");}
    acpi::init(boot::rsdp_request.response,boot::hhdm_request.response);
    pci::enumerate_readonly();
    net::nic::probe_readonly();
    security::itco::probe_readonly();
    ahci::inspect_readonly();
    storage::init();
    require_watchdog(security::watchdog::Stage::devices_ready);

    auto disk=disk_probe::run_qemu_gpt_probe();volume::init();
    bool live_catalog=false;
    if(disk.selected!=gpt::Selection::none&&disk_probe::has_valid_table()){live_catalog=volume::bind_gpt(0,disk_probe::last_table());serial::writeln(live_catalog?"Live volume catalog: READY":"Live volume catalog: REJECTED");}
    recovery_probe::Result recovery{}; if(live_catalog) recovery=recovery_probe::run(disk.disk_guid);
    recovery_journal_probe::Result rjournal{};if(live_catalog)rjournal=recovery_journal_probe::run(disk.disk_guid);
    if(features::recovery_journal_write_live&&rjournal.write_test_attempted&&!rjournal.write_test_passed)panic::stop("Peregrinus recovery journal persistence test failed");
    if(rjournal.assessment.split_brain)panic::stop("Peregrinus recovery journal split-brain");
    require_watchdog(security::watchdog::Stage::recovery_ready);

    const auto policy=boot_policy::evaluate(disk,volume::catalog(),recovery);
    serial::write("Boot health: ");serial::writeln(boot_policy::health_name(policy.health));
    serial::write("Noe policy action: ");serial::writeln(boot_policy::action_name(policy.action));
    serial::writeln(features::firewall_default_deny?"Firewall default policy: DENY":"Firewall default policy: INVALID");
    serial::writeln(features::firewall_frame_hook_foundation?"Firewall Ethernet/IPv4 hook: READY (synthetic/test path)":"Firewall Ethernet/IPv4 hook: INVALID");
    serial::writeln(features::firewall_packet_hook_live?"Firewall NIC datapath hook: LIVE":"Firewall NIC datapath hook: BLOCKED");
    const auto guard=security::guard_policy::evaluate(security::guard_policy::IntegrityState::trusted,policy);
    serial::write("Peregrinus Guard action: ");serial::writeln(security::guard_policy::action_name(guard.action));
    serial::write("Rollback eligibility: ");serial::writeln(guard.rollback_eligible?"ELIGIBLE":"NO");
    serial::write("Running authenticated slot: ");serial::writeln(security::root_trust::slot_name(security::root_trust::running_slot()));
    serial::write("Purgatorio component admission: ");serial::writeln(security::quarantine::registry().saturated()?"FAIL-CLOSED/SATURATED":"READY/EMPTY");
    require_watchdog(security::watchdog::Stage::policy_ready);

    if(guard.action==security::guard_policy::Action::halt_fail_closed){serial::writeln("PEREGRINUS GUARD: fail-closed halt.");halt_forever();}
    if(guard.action==security::guard_policy::Action::recovery_readonly)serial::writeln("PEREGRINUS GUARD: recovery/read-only path only.");
    if(guard.action==security::guard_policy::Action::boot_readonly)serial::writeln("PEREGRINUS GUARD: trusted read-only system path allowed.");
    require_watchdog(security::watchdog::Stage::complete);
    const uint8_t running_slot=security::root_trust::running_slot()==security::root_trust::Slot::last_known_good?PEREGRINUS_RJ_SLOT_LKG:PEREGRINUS_RJ_SLOT_CURRENT;
    const auto confirm=live_catalog?recovery_journal_probe::confirm_boot_success(disk.disk_guid,running_slot,security::root_trust::build_generation,security::root_trust::security_epoch):recovery_journal_probe::CommitResult::recovery_unavailable;
    serial::write("IA_RECOVERY direct boot-success commit: ");serial::writeln(recovery_journal_probe::commit_result_name(confirm));
    if(features::recovery_journal_commit_live&&confirm!=recovery_journal_probe::CommitResult::confirmed)panic::stop("Peregrinus direct recovery-journal success commit failed");
    hw::init();hw::print();
    if constexpr(features::nic_driver_live){
        serial::writeln("Purgatorio 0.1 qualification profile: starting gated e1000 polling datapath.");
        if(!net::sandbox::service().init())panic::stop("Muro e1000 qualification init failed");
        for(;;){const size_t n=net::sandbox::service().poll(8);if(n==0)asm volatile("pause");}
    }
    serial::writeln("Purgatorio 0.1 SAFE: firewall/network stack present, live NIC ownership remains BLOCKED.");halt_forever();
}
