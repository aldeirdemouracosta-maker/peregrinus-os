# Peregrinus OS — Architecture after Muro 1.0

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
The Makefile pins tool *names* (`clang++`, `ld.lld`), not versions. Different clang releases produce different `.text` and therefore different seals; reproducible artifacts require a pinned toolchain (see the CI suggestion in the project review).
