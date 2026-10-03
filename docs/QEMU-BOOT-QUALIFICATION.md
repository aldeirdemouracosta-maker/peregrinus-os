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

## Full trusted chain (`make qemu-trusted-chain-test`)

OVMF with Secure Boot enforced (Ubuntu's public *snakeoil* test keys) → signed boot controller (`make preboot-recovery-controller`, TPM commit base 41 / next 42) → swtpm NV counter `0x0180F050` → IA_RECOVERY pre-boot journal attempt → signed Limine with its config BLAKE2B enrolled → BLAKE2b-pinned recovery-live kernel → boot-success commit. The disk is the disposable GPT image plus an ESP (`create-gpt-test-image.py --esp-mib`).

| Scenario | Required result |
|---|---|
| counter 42 (COMMITTED), booted twice on the same disk | `Secure Boot: ACTIVE`, `TPM state: COMMITTED`, pre-boot journal selects CURRENT, kernel `.text` PASS, generation 23, **`IA_RECOVERY direct boot-success commit: CONFIRMED`** both times |
| counter 40 (STALE) and 43 (FUTURE) | controller: **`TPM counter incompatible with media; FAIL-CLOSED`**; no loader, no kernel |
| one flipped byte in the kernel on the ESP | controller chainloads Limine; the **kernel never starts** (enrolled-config hash pin) |
| unsigned controller | firmware: **`Access Denied`**; nothing runs |
| counter 41 (STAGED) | `TPM transition still STAGED`; staged Limine boots the kernel, no pre-boot attempt → kernel **`MARK-REJECTED`** + fail-closed halt |
| no TPM | `TPM anchor unavailable/invalid; CURRENT-only path` → kernel **`MARK-REJECTED`** + fail-closed halt |
| Secure Boot off (plain OVMF), counter COMMITTED | `Secure Boot: NOT TRUSTED; CURRENT-only path` → kernel **`MARK-REJECTED`** + fail-closed halt |

Every degraded path (STAGED, no TPM, no Secure Boot) still loads the kernel, but the controller records no pre-boot attempt, so the recovery-live kernel refuses to mark the boot healthy and halts. The TPM counter is provisioned by the operator tool `scripts/provision-tpm-counter.sh` (see below), so that tool is exercised by every run.

Test keys only; nothing produced here is a release artifact.

### Findings from the first trusted-chain run

- **recovery-live could never complete a boot.** Its read-only GPT probe was gated on `disposable_qemu_disk_only`, so on any non-QEMU-test disk boot health was UNAVAILABLE and the Guard halted before the boot-success commit. The probe now uses `recovery_metadata_live`, the gate the anchor/journal readers already used (`tests/safety.sh` checks it). SAFE still never reads storage.
- **Provision the counter index with `no_da`.** With an empty authValue, dictionary-attack protection adds nothing, but after an unclean shutdown it returns `TPM_RC_LOCKOUT` (0x921) to the controller's `NV_Read`, which silently degrades the boot to the CURRENT-only path. The test provisions `nt=counter|ownerwrite|authread|ownerread|no_da`.
- `scripts/prepare-epoch-transition.sh` still looked for Limine under `third_party/limine/`; it now uses the pinned build in `third_party/limine-bin`.

### TPM counter provisioning

`scripts/provision-tpm-counter.sh` verifies (default) or defines (`--define`) NV index `0x0180F050` and fails closed on wrong attributes: counter type, OWNERWRITE, AUTHREAD, no AUTHWRITE and **NO_DA**. `--advance-to N` is for test TPMs only, since NV counters never decrease. It uses the standard `TPM2TOOLS_TCTI`.

### STAGED is deliberately journal-free

In the STAGED state (an epoch transition waiting for its TPM commit) the controller prints `TPM transition still STAGED; journal auto-fallback disabled` and records no attempt, and LKG is reachable only through an explicit authenticated boot request (`peregrinus_tpm_state_allows_lkg`). Journaling attempts there would re-enable automatic fallback across an epoch boundary. Consequence, pinned by the test: the staged kernel of a transition must not be a commit-live build, because a recovery-live kernel always refuses the commit and halts in STAGED.

## Still not run

- A real cross-epoch transition (CURRENT epoch > LKG epoch) with the staged LKG one-shot request; the current release profile is same-epoch, so `prepare-epoch-transition.sh` stops by design.
- Physical X79/iTCO/e1000 hardware and real TPM chips.
