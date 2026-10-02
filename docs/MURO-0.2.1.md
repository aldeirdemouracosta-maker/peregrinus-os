# Muro das Lamentações 0.2.1 — Lean/Hardened

This checkpoint is a cleanup/hardening release made before adding e1000 RX.

## Removed from active runtime

- Boot Control A/B parser/probe and duplicate rollback planner.
- Kernel boot-time development self-tests.
- Unused Limine firmware/EFI runtime requests.
- unconditional 2 MiB DMA32 reservation in SAFE.
- six cached PCI BAR records per retained device.
- wildcard `find kernel -name '*.cpp'` build discovery.

## Added/changed

- one authoritative `include/peregrinus/release_profile.h`;
- CURRENT generation 13, LKG generation 12, both epoch 3;
- real Muro 0.2 gen12 binaries rebuilt as `LAST-KNOWN-GOOD`;
- clang/clang++/ld.lld pinned in Makefile;
- `-ffunction-sections -fdata-sections` + `--gc-sections`;
- explicit kernel source list;
- tiny freestanding `memset/memcpy/memmove` runtime;
- compact selected PCI catalog with BAR lookup on demand;
- TPM epoch-transition preparation refuses same-epoch releases.

## On-disk compatibility

The old Boot Control sectors (`IA_RECOVERY +1` and `last-1`) are kept reserved and zeroed in new test images. Journal A/B remains at `+2` and `last-2`, so the recovery layout does not need to be reformatted.
