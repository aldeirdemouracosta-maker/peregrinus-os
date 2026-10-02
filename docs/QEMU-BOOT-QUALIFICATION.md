# QEMU boot qualification

`make qemu-boot-test` (and the `qemu-boot` CI job) boots each profile in QEMU q35 with UEFI (OVMF), headless, and asserts the serial log. Limine is the pinned v12.9.1 release tarball, verified by SHA-256 and built by `scripts/fetch-limine.sh`.

| Profile | Media | Required result |
|---|---|---|
| `safe` | SAFE kernel, no disk | `.text` seal PASS, NIC BLOCKED, boot health UNAVAILABLE, **Guard halts fail-closed** |
| `disk` | `dma-test` kernel + disposable GPT image (read-only, `snapshot=on`) | redundant GPT HEALTHY, recovery anchors A/B VALID, **BOOT-READONLY**, NIC BLOCKED |
| `e1000` | e1000 qualification kernel + the same read-only disk, QEMU `socket` netdev | e1000 rings READY; a peer (`tests/qemu_net_peer.py`) gets an **ARP reply**, an **ICMP echo reply** (id/seq/payload/checksums verified) and **no reply to non-allowlisted UDP** |
| `journal` | recovery-journal write-test kernel, **writable** copy of the disk, booted twice | each boot: attempt+success transaction **PASS**, no split-brain, the disk image actually changes |
| `commit` | recovery-commit kernel, writable disk with a pending CURRENT attempt (`create-gpt-test-image.py --pending-current-attempt`) | 1st boot **CONFIRMED**; 2nd boot on the same disk (no new attempt) **MARK-REJECTED** + fail-closed panic; baseline disk (never attempted) **MARK-REJECTED** |

Any `PEREGRINUS KERNEL PANIC` or `CPU EXCEPTION` fails the test, except in the `commit` steps that require the fail-closed panic. The `disk` profile also checks that the read-only run leaves the image byte-identical.

## Findings from the first runtime boot

- The SAFE build never reads storage, so it has no verified boot health and the Guard halts fail-closed by design. A SAFE boot that continues requires a profile with verified (read-only) storage.
- The e1000 profile previously excluded the read-only `PEREGRINUS_QEMU_DMA_TEST` profile. Without verified boot health the Guard halted before the NIC datapath, so the live e1000 path was unreachable. The e1000 target now includes the read-only disposable-disk profile; all storage-**write** profiles stay mutually exclusive with e1000 at compile time.

- A direct boot-success commit is only legal after the pre-boot controller recorded a pending attempt. On the stock test image the commit kernel correctly refuses (`MARK-REJECTED`) and panics; the positive path needs the `--pending-current-attempt` image that mimics the controller.

## Still not run

- Full trusted chain in QEMU: UEFI boot controller → recovery journal attempt → Limine staged/committed config, with the swtpm NV counter (epoch floor / commit).
- Physical X79/iTCO/e1000 hardware.
