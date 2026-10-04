# Peregrinus OS — Architecture (Purgatório 0.1.1)

The project remains layered so that security-sensitive state is not mixed with experimental networking.

1. **Boot / Root of Trust** — UEFI Secure Boot, pinned Limine configuration/file hashes, security epoch and TPM monotonic anchor.
2. **Recovery** — GPT redundancy, Recovery Anchor A/B and Recovery Journal A/B with fail-closed split-brain handling.
3. **Kernel foundation** — x86-64 exceptions, PMM, dedicated UC MMIO mapping, PCI discovery and bounded DMA32 when a profile requires it.
4. **Peregrinus Guard / Muro** — default-deny rule engine, bounded Ethernet/IPv4 parser, stateful reply guard, burst guard and final datapath audit.
5. **NIC qualification** — e1000 polling RX/TX exists only behind `PEREGRINUS_QEMU_E1000_SANDBOX=1`.

The production SAFE image intentionally stops at layer 4 and never takes NIC ownership. This preserves a small attack surface while the qualification image can exercise the hardware-facing code independently.

## Integrity manifest scope (clarification)
The embedded `.peregrinus_integrity` manifest is an unkeyed SHA-256 of `.text` stored inside the same ELF. It detects corruption and accidental modification, not a deliberate attacker, who can re-run `scripts/seal-kernel.py`. Authenticity comes from the Secure Boot → trusted boot controller → pinned Limine config hash chain. `.rodata`/`.data` are not covered by the manifest.

## Toolchain reproducibility
The Makefile pins tool *names* (`clang++`, `ld.lld`); CI pins the version (Ubuntu 24.04 LLVM 18). With that toolchain the build is bit-for-bit reproducible (no debug info, `/Brepro` for the EFI controller, explicit source lists). `scripts/make-artifacts.sh` rebuilds every committed binary and CI fails if `artifacts/SHA256SUMS` differs; on `main` CI also publishes GitHub build-provenance attestations for the rebuilt binaries.

## Kernel stack and exceptions (0.1.1)
`kmain` switches early to a 64 KiB kernel stack mapped in PML4 slot 509 with an unmapped guard page below it. Overflows fault instead of corrupting memory; #DF, NMI and #MC run on dedicated IST stacks from the TSS, so the fault is reported on the serial port instead of ending in a silent triple fault. All kernel code is built with `-fstack-protector-strong` (global canary seeded from RDRAND, TSC fallback).
