# Peregrinus OS — Purgatorio 0.1 Admission Gate
# Toolchain is intentionally pinned instead of inheriting CC/CXX/LD from the host.
CXX := clang++
CC  := clang
LD  := ld.lld
PYTHON := python3

BUILD ?= build
KERNEL := $(BUILD)/peregrinus.elf
PROFILE := scripts/release-profile.py
CURRENT_GENERATION := $(shell $(PROFILE) current_generation)
LKG_GENERATION := $(shell $(PROFILE) lkg_generation)
CURRENT_EPOCH := $(shell $(PROFILE) current_epoch)
LKG_EPOCH := $(shell $(PROFILE) lkg_epoch)
MIN_EPOCH := $(shell $(PROFILE) min_epoch)
RELEASE_TAG := $(shell $(PROFILE) tag)

CPPFLAGS := -Iinclude -Ikernel
EXTRA_CPPFLAGS ?=
SEAL_LABEL ?= $(RELEASE_TAG)
CXXFLAGS := -std=c++23 -ffreestanding -fno-exceptions -fno-rtti -fstack-protector-strong -mstack-protector-guard=global -fno-pic -mno-red-zone -mcmodel=kernel -O2 -Wall -Wextra -Werror -mgeneral-regs-only -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-threadsafe-statics -ffunction-sections -fdata-sections
ASFLAGS  := -ffreestanding -mno-red-zone -mgeneral-regs-only -ffunction-sections -fdata-sections
LDFLAGS  := -nostdlib -z max-page-size=0x1000 --gc-sections -T linker.ld

CPP_SRCS := \
	kernel/acpi/acpi.cpp \
	kernel/arch/x86_64/cpu.cpp \
	kernel/arch/x86_64/exceptions.cpp \
	kernel/arch/x86_64/gdt.cpp \
	kernel/arch/x86_64/idt.cpp \
	kernel/arch/x86_64/kstack.cpp \
	kernel/boot/limine_requests.cpp \
	kernel/console/format.cpp \
	kernel/console/framebuffer.cpp \
	kernel/console/serial.cpp \
	kernel/console/text_console.cpp \
	kernel/hw/manifest.cpp \
	kernel/input/keyboard.cpp \
	kernel/input/ps2.cpp \
	kernel/main.cpp \
	kernel/mm/memory.cpp \
	kernel/mm/mmio.cpp \
	kernel/mm/paging.cpp \
	kernel/net/ethernet_ipv4.cpp \
	kernel/net/nic_probe.cpp \
	kernel/panic/panic.cpp \
	kernel/pci/pci.cpp \
	kernel/security/firewall.cpp \
	kernel/security/guard_policy.cpp \
	kernel/security/integrity.cpp \
	kernel/security/quarantine.cpp \
	kernel/security/itco_watchdog.cpp \
	kernel/security/root_trust.cpp \
	kernel/security/sha256.cpp \
	kernel/security/watchdog.cpp \
	kernel/runtime/memory.cpp \
	kernel/runtime/stack_protector.cpp \
	kernel/shell/shell.cpp \
	kernel/storage/ahci.cpp \
	kernel/storage/boot_policy.cpp \
	kernel/storage/disk_probe.cpp \
	kernel/storage/gpt.cpp \
	kernel/storage/identify.cpp \
	kernel/storage/recovery_anchor.cpp \
	kernel/storage/recovery_journal.cpp \
	kernel/storage/recovery_journal_probe.cpp \
	kernel/storage/recovery_probe.cpp \
	kernel/storage/storage.cpp \
	kernel/storage/volume.cpp

E1000_CPP_SRCS := \
	kernel/net/e1000_model.cpp \
	kernel/net/e1000.cpp \
	kernel/net/arp.cpp \
	kernel/net/icmp.cpp \
	kernel/net/stateful_guard.cpp \
	kernel/net/burst_guard.cpp \
	kernel/net/datapath.cpp \
	kernel/net/sandbox_service.cpp

EXTRA_CPP_SRCS ?=

ASM_SRCS := \
	kernel/arch/x86_64/isr.S \
	kernel/start.S

OBJS := $(CPP_SRCS:%.cpp=$(BUILD)/%.o) $(EXTRA_CPP_SRCS:%.cpp=$(BUILD)/%.o) $(ASM_SRCS:%.S=$(BUILD)/%.o)

all: $(KERNEL)

# -MMD -MP: every object also depends on the headers it includes, so a changed struct
# layout can never be linked against stale objects compiled with the old one.
$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(EXTRA_CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(EXTRA_CPPFLAGS) $(ASFLAGS) -MMD -MP -c $< -o $@

-include $(OBJS:.o=.d)

$(KERNEL): $(OBJS) linker.ld scripts/seal-kernel.py
	$(LD) $(LDFLAGS) $(OBJS) -o $@
	./scripts/seal-kernel.py $@ $(SEAL_LABEL)

clean:
	rm -rf build build-*

check: $(KERNEL)
	./tests/elf_sanity.sh $(KERNEL)
	./tests/safety.sh
	./tests/profile_matrix.sh
	./tests/release_profile.sh
	./tests/identify_parser.sh
	./tests/gpt_redundancy.sh
	./tests/recovery_anchor.sh
	./tests/boot_policy.sh
	./tests/sha256.sh
	./tests/watchdog.sh
	./tests/guard_policy.sh
	./tests/integrity_seal.sh $(KERNEL)
	./tests/integrity_tamper.sh $(KERNEL)
	./tests/root_trust_config.sh
	./tests/trusted_boot_controller.sh
	./tests/tpm_monotonic.sh
	./tests/tpm_commit.sh
	./tests/epoch_commit_ceremony.sh
	./tests/recovery_journal.sh
	./tests/recovery_commit_powerfail.sh
	./tests/ahci_timeout.sh
	./tests/e1000_driver.sh
	./tests/acpi_tables.sh
	./tests/text_console.sh
	./tests/shell.sh
	./tests/preboot_recovery.sh
	./tests/preboot_controller_binary.sh
	./tests/firewall.sh
	./tests/quarantine.sh
	./tests/network_parser.sh
	./tests/muro_remaining_stages.sh
	./tests/muro_adversarial.sh
	./tests/safe_binary_isolation.sh $(KERNEL)

iso: $(KERNEL)
	./scripts/make-iso.sh safe

qemu: iso
	./scripts/run-qemu.sh safe

dma-test-kernel:
	$(MAKE) BUILD=build-qemu-dma EXTRA_CPPFLAGS=-DPEREGRINUS_QEMU_DMA_TEST=1 all

qemu-test-disk:
	./scripts/create-gpt-test-image.py build/peregrinus-testdisk.img
	./scripts/verify-gpt-test-image.py build/peregrinus-testdisk.img

current-slot:
	$(MAKE) BUILD=build-current SEAL_LABEL=$(RELEASE_TAG)-CURRENT EXTRA_CPPFLAGS="-DPEREGRINUS_SLOT_CURRENT=1" all

lkg-slot:
	@test -f lkg-reference/peregrinus-muro-1.0.1-gen22-safe.elf
	@echo "LKG: retained generation $(LKG_GENERATION) LKG-slot artifact"

secure-slots: current-slot lkg-slot
	rm -rf build-secure
	mkdir -p build-secure/boot/limine
	cp build-current/peregrinus.elf build-secure/boot/peregrinus-current.elf
	cp lkg-reference/peregrinus-muro-1.0.1-gen22-safe.elf build-secure/boot/peregrinus-lkg.elf
	./scripts/generate-epoch-configs.py --current build-secure/boot/peregrinus-current.elf --lkg build-secure/boot/peregrinus-lkg.elf --staged-output build-secure/boot/limine/limine-staged.conf --committed-output build-secure/boot/limine/limine-committed.conf --current-generation $(CURRENT_GENERATION) --lkg-generation $(LKG_GENERATION) --current-epoch $(CURRENT_EPOCH) --lkg-epoch $(LKG_EPOCH)
	./scripts/verify-epoch-configs.py --staged build-secure/boot/limine/limine-staged.conf --committed build-secure/boot/limine/limine-committed.conf --root build-secure --committed-lkg

trusted-boot-controller:
	./scripts/build-trusted-boot-controller.sh

recovery-journal-test-kernel:
	$(MAKE) BUILD=build-recovery-journal-test SEAL_LABEL=$(RELEASE_TAG)-RECOVERY-JOURNAL-TEST EXTRA_CPPFLAGS="-DPEREGRINUS_QEMU_RECOVERY_JOURNAL_TEST=1 -DPEREGRINUS_SLOT_CURRENT=1" all

recovery-commit-test-kernel:
	$(MAKE) BUILD=build-recovery-commit-test SEAL_LABEL=$(RELEASE_TAG)-RECOVERY-COMMIT-TEST EXTRA_CPPFLAGS="-DPEREGRINUS_QEMU_RECOVERY_COMMIT_TEST=1 -DPEREGRINUS_SLOT_CURRENT=1" all

preboot-recovery-controller:
	EXTRA_CFLAGS="-DPEREGRINUS_RECOVERY_JOURNAL_LIVE=1 -DPEREGRINUS_TPM_COMMIT_BASE=41ull -DPEREGRINUS_TPM_COMMIT_NEXT=42ull" ./scripts/build-trusted-boot-controller.sh build-bootctl-preboot

current-recovery-live:
	$(MAKE) BUILD=build-current-recovery-live SEAL_LABEL=$(RELEASE_TAG)-CURRENT-RECOVERY-LIVE EXTRA_CPPFLAGS="-DPEREGRINUS_RECOVERY_COMMIT_LIVE=1 -DPEREGRINUS_SLOT_CURRENT=1" all

lkg-recovery-live:
	@test -f lkg-reference/peregrinus-muro-1.0.1-gen22-recovery-live.elf
	@echo "LKG: retained generation $(LKG_GENERATION) recovery-live LKG-slot artifact"

recovery-live-slots: current-recovery-live lkg-recovery-live
	rm -rf build-recovery-live-secure
	mkdir -p build-recovery-live-secure/boot/limine
	cp build-current-recovery-live/peregrinus.elf build-recovery-live-secure/boot/peregrinus-current.elf
	cp lkg-reference/peregrinus-muro-1.0.1-gen22-recovery-live.elf build-recovery-live-secure/boot/peregrinus-lkg.elf
	./scripts/generate-epoch-configs.py --current build-recovery-live-secure/boot/peregrinus-current.elf --lkg build-recovery-live-secure/boot/peregrinus-lkg.elf --staged-output build-recovery-live-secure/boot/limine/limine-staged.conf --committed-output build-recovery-live-secure/boot/limine/limine-committed.conf --current-generation $(CURRENT_GENERATION) --lkg-generation $(LKG_GENERATION) --current-epoch $(CURRENT_EPOCH) --lkg-epoch $(LKG_EPOCH)
	./scripts/verify-epoch-configs.py --staged build-recovery-live-secure/boot/limine/limine-staged.conf --committed build-recovery-live-secure/boot/limine/limine-committed.conf --root build-recovery-live-secure --committed-lkg


double-fault-test-kernel:
	$(MAKE) BUILD=build-double-fault-test SEAL_LABEL=$(RELEASE_TAG)-DIAG-STACK EXTRA_CPPFLAGS="-DPEREGRINUS_DIAG_STACK_OVERFLOW=1" all

qemu-qualify:
	./scripts/qemu-qualify.sh

qemu-e1000-sandbox:
	$(MAKE) BUILD=build-qemu-e1000 SEAL_LABEL=$(RELEASE_TAG)-QEMU-E1000 EXTRA_CPPFLAGS="-DPEREGRINUS_QEMU_E1000_SANDBOX=1 -DPEREGRINUS_SLOT_CURRENT=1" EXTRA_CPP_SRCS="$(E1000_CPP_SRCS)" all

.PHONY: all clean check double-fault-test-kernel qemu-qualify iso qemu dma-test-kernel qemu-test-disk current-slot lkg-slot secure-slots trusted-boot-controller recovery-journal-test-kernel recovery-commit-test-kernel preboot-recovery-controller current-recovery-live lkg-recovery-live recovery-live-slots qemu-e1000-sandbox
