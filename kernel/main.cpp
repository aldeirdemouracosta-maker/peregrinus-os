#include <stdint.h>
#include <peregrinus/release_profile.h>
#include "console/serial.hpp"
#include "console/framebuffer.hpp"
#include "console/text_console.hpp"
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
#include "net/nic_probe.hpp"
#include "net/sandbox_service.hpp"
#include "storage/recovery_journal.hpp"
#include "storage/recovery_journal_probe.hpp"
#include "config/features.hpp"
#include "arch/x86_64/kstack.hpp"
#include "input/keyboard.hpp"
#include "input/ps2.hpp"
#include "shell/shell.hpp"
#include "arch/x86_64/interrupts.hpp"

// A profile that commits boot success must also be able to read the GPT and the journal;
// otherwise the commit can never be confirmed and every boot ends in a panic.
static_assert(!peregrinus::features::recovery_journal_commit_live||(peregrinus::features::ahci_dma_read_live&&peregrinus::features::recovery_journal_read_live),"recovery commit profile without a GPT/journal read path");

// The image epoch is a build-time constant: check it at build time instead of pretending to
// verify it at runtime. Boot-time epoch enforcement belongs to the UEFI controller + TPM.
static_assert(peregrinus::security::root_trust::security_epoch>=peregrinus::security::root_trust::minimum_security_epoch,"image security epoch below the minimum");

static void halt_forever(){for(;;)asm volatile("hlt");}
static void require_watchdog(peregrinus::security::watchdog::Stage s){if(!peregrinus::security::watchdog::checkpoint(s))peregrinus::panic::stop("Peregrinus boot watchdog sequence violation");}

static void kmain_stage2();
static void run_shell(const peregrinus::shell::SystemInfo& info);
static const char* current_layout();
extern "C" const char* peregrinus_stack_guard_source;

extern "C" void kmain(){
    using namespace peregrinus;
    security::watchdog::reset();require_watchdog(security::watchdog::Stage::boot_entry);
    serial::init();
    // Screen console first, so the whole boot log (and any panic) is readable without a serial cable.
    if(boot::framebuffer_request.response&&boot::framebuffer_request.response->framebuffer_count&&framebuffer::init(boot::framebuffer_request.response->framebuffers[0])&&text_console::init())serial::set_mirror(text_console::putc);
    serial::writeln("");serial::writeln("====================================");serial::writeln(" Peregrinus OS — " PEREGRINUS_RELEASE_NAME);serial::writeln("====================================");
    if(!LIMINE_BASE_REVISION_SUPPORTED(boot::base_revision))panic::stop("Limine base revision unsupported");
    serial::writeln(text_console::ready()?"Console: framebuffer text (mirror of serial)":"Console: serial only (no usable framebuffer)");
    if(boot::bootloader_info_request.response){serial::write("Bootloader: ");serial::write(boot::bootloader_info_request.response->name);serial::write(" ");serial::writeln(boot::bootloader_info_request.response->version);}
    cpu::report();gdt::init();idt::init();require_watchdog(security::watchdog::Stage::cpu_ready);

    memory::init_from_limine(boot::memmap_request.response);
    memory::set_hhdm_offset(boot::hhdm_request.response?boot::hhdm_request.response->offset:0);
    memory::init_page_allocator(boot::memmap_request.response);
    if(!paging::init())serial::writeln("WARNING: Peregrinus page mapper unavailable; MMIO will remain blocked");
    if(!mmio::init())serial::writeln("WARNING: dedicated MMIO mapper unavailable");
    uint64_t stack_top=0;
    if(!kstack::create(stack_top))panic::stop("guard-page kernel stack unavailable");
    serial::writeln("Kernel stack: 64 KiB with unmapped guard page (PML4 slot 509)");
    peregrinus_switch_stack(stack_top,kmain_stage2);
}

#if defined(PEREGRINUS_DIAG_STACK_OVERFLOW) && PEREGRINUS_DIAG_STACK_OVERFLOW == 1
// Qualification only: recurse until the guard page below the kernel stack is hit.
__attribute__((noinline)) static unsigned overflow_stack(unsigned depth){volatile char pad[512];pad[0]=char(depth);if(depth==0xffffffffu)return 0;return overflow_stack(depth+1)+pad[0];}
#endif

static void kmain_stage2(){
    using namespace peregrinus;
    {serial::write("Stack protector: ACTIVE, canary from ");serial::writeln(peregrinus_stack_guard_source);}
#if defined(PEREGRINUS_DIAG_STACK_OVERFLOW) && PEREGRINUS_DIAG_STACK_OVERFLOW == 1
    serial::writeln("DIAGNOSTIC: overflowing the kernel stack on purpose");
    (void)overflow_stack(0);
#endif
    require_watchdog(security::watchdog::Stage::memory_ready);

    serial::write("Compiled slot (not attested by the kernel): ");serial::writeln(security::root_trust::slot_name(security::root_trust::running_slot()));
    {char g[24];format::dec64(security::root_trust::build_generation,g);serial::write("Build generation: ");serial::writeln(g);}
    {char e[24];format::dec64(security::root_trust::security_epoch,e);serial::write("Security epoch: ");serial::writeln(e);}
    const auto integrity=security::integrity::verify_kernel_text();
    serial::write("Kernel integrity manifest: ");serial::writeln(integrity.manifest_valid?"VALID":"INVALID");
    serial::write("Kernel .text SHA-256 (corruption check, unsigned): ");serial::writeln(integrity.trusted()?"PASS":"FAIL");
    if(!integrity.trusted())panic::stop("Peregrinus Guard kernel integrity failure");
    require_watchdog(security::watchdog::Stage::integrity_ready);

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
    serial::write("Disk policy action: ");serial::writeln(boot_policy::action_name(policy.action));
    serial::writeln("Firewall default policy: DENY (allowlist)");
    serial::writeln(features::firewall_packet_hook_live?"Firewall NIC datapath hook: LIVE":"Firewall NIC datapath hook: BLOCKED");
    const auto guard=security::guard_policy::evaluate(security::guard_policy::IntegrityState::trusted,policy,features::storage_policy_required);
    serial::write("Peregrinus Guard action: ");serial::writeln(security::guard_policy::action_name(guard.action));
    serial::write("Rollback eligibility: ");serial::writeln(guard.rollback_eligible?"ELIGIBLE":"NO");
    serial::write("Purgatorio component admission: ");serial::writeln(security::quarantine::registry().saturated()?"FAIL-CLOSED/SATURATED":"READY/EMPTY");
    require_watchdog(security::watchdog::Stage::policy_ready);

    if(guard.action==security::guard_policy::Action::halt_fail_closed){serial::writeln("PEREGRINUS GUARD: fail-closed halt.");halt_forever();}
    if(guard.action==security::guard_policy::Action::recovery_readonly)serial::writeln("PEREGRINUS GUARD: recovery/read-only path only.");
    if(guard.action==security::guard_policy::Action::boot_readonly)serial::writeln("PEREGRINUS GUARD: trusted read-only system path allowed.");
    if(guard.action==security::guard_policy::Action::boot_passive)serial::writeln("PEREGRINUS GUARD: passive boot; this build has no storage path.");
    require_watchdog(security::watchdog::Stage::complete);
    const uint8_t running_slot=security::root_trust::running_slot()==security::root_trust::Slot::last_known_good?PEREGRINUS_RJ_SLOT_LKG:PEREGRINUS_RJ_SLOT_CURRENT;
    const auto confirm=live_catalog?recovery_journal_probe::confirm_boot_success(disk.disk_guid,running_slot,security::root_trust::build_generation,security::root_trust::security_epoch):recovery_journal_probe::CommitResult::recovery_unavailable;
    serial::write("IA_RECOVERY direct boot-success commit: ");serial::writeln(recovery_journal_probe::commit_result_name(confirm));
    if(features::recovery_journal_commit_live&&confirm!=recovery_journal_probe::CommitResult::confirmed)panic::stop("Peregrinus direct recovery-journal success commit failed");
    hw::init();hw::print();
    if constexpr(features::nic_driver_live){
        serial::writeln("e1000 qualification profile: starting gated polling datapath.");
        if(!net::sandbox::service().init())panic::stop("e1000 qualification init failed");
    } else {
        serial::writeln("Boot complete: firewall/network stack present, live NIC ownership remains BLOCKED in this build.");
    }
    const auto& m=hw::manifest();
    const bool kbd=ps2::init();
    const bool irq=interrupts::init(kbd,serial::present());
    serial::writeln(irq?"Interrupts: PIC/PIT live (100 Hz timer, keyboard and COM1 IRQs); idle CPU halts":"Interrupts: no timer ticks; staying in polling mode (fail-closed fallback)");
    const shell::SystemInfo info{PEREGRINUS_RELEASE_NAME,security::root_trust::build_generation,security::root_trust::security_epoch,
        security::guard_policy::action_name(guard.action),integrity.trusted(),peregrinus_stack_guard_source,text_console::ready(),kbd,
        m.usable_memory_bytes/(1024*1024),m.pci_functions,m.local_apics,m.io_apics,m.madt_present,m.mcfg_present,
        irq,interrupts::timer_hz,interrupts::ticks,current_layout};
    run_shell(info);
}

// Input loop (interrupts stay off, so everything is polled): keyboard and serial feed one line
// editor; the e1000 qualification datapath, when present, is serviced in the same loop.
static void shell_out(const char* s){peregrinus::serial::write(s);}
static peregrinus::keyboard::Decoder g_keys;
static const char* current_layout(){return peregrinus::keyboard::layout_name(g_keys.layout());}
static void run_shell(const peregrinus::shell::SystemInfo& info){
    using namespace peregrinus;
    serial::writeln(info.keyboard?"Shell: keyboard (PS/2) and serial input. Type 'ajuda'.":"Shell: serial input only (no PS/2 controller). Type 'ajuda'.");
    g_keys.reset();shell::LineEditor ed;
    serial::write(shell::prompt());
    for(;;){
        bool idle=true;
        if constexpr(features::nic_driver_live){if(net::sandbox::service().poll(8))idle=false;}
        // Serial bytes are UTF-8; keyboard characters are Latin-1 (up to two per scancode).
        char keys[2];uint8_t nkeys=0,sc=0,b=0;char sb=0;bool from_serial=false,line_done=false;
        if(interrupts::active()){
            if(interrupts::next_serial_byte(b)){idle=false;from_serial=true;sb=static_cast<char>(b);}
            else if(interrupts::next_scancode(sc)){idle=false;nkeys=g_keys.feed(sc,keys);}
        }else{
            if(serial::poll_input(sb)){idle=false;from_serial=true;}
            else if(ps2::poll(sc)){idle=false;nkeys=g_keys.feed(sc,keys);}
        }
        if(from_serial)line_done=ed.feed_utf8(sb,shell_out);
        for(uint8_t i=0;i<nkeys&&!line_done;++i)line_done=ed.feed(keys[i],shell_out);
        if(!from_serial&&nkeys==0){
            if(idle){
                // The e1000 qualification datapath is polled, so that profile keeps spinning.
                if constexpr(features::nic_driver_live)asm volatile("pause");
                else interrupts::idle_wait();
            }
            continue;
        }
        if(!line_done)continue;
        const auto action=shell::execute(ed.line(),info,shell_out);
        ed.clear();
        if(action==shell::Action::clear_screen)text_console::clear();
        if(action==shell::Action::reboot){ps2::request_reset();serial::writeln("Reset was ignored by the hardware; halting.");halt_forever();}
        if(action==shell::Action::halt)halt_forever();
        if(action==shell::Action::layout_us)g_keys.set_layout(keyboard::Layout::us);
        if(action==shell::Action::layout_abnt2)g_keys.set_layout(keyboard::Layout::abnt2);
        serial::write(shell::prompt());
    }
}
