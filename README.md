# Peregrinus OS — Peregrinus Guard — Purgatório 0.1.1 Admission Gate

Muro 1.0 closes the first firewall/network-foundation cycle on top of the Lean/Hardened 0.2.1 checkpoint. The production **SAFE** kernel remains passive: it parses and enforces policy only on in-memory/test frames and does not own a NIC, map an Ethernet BAR, enable NIC bus mastering, reserve DMA32 for networking, or transmit packets.

A separate **QEMU e1000 qualification profile** contains the live datapath. It is deliberately narrow: Intel 82540EM/e1000 (`8086:100e`), polling only, 8 RX + 8 TX descriptors, static IPv4 `10.0.2.15`, ARP response, ICMP echo response, default-deny IPv4 policy, stateful reply guard for TCP/UDP, event-window burst protection, and bounded audit.

## Release profile

- CURRENT: Purgatório 0.1.1, generation **24**, security epoch **3**
- LKG: Muro 1.0.1 generation **22**, security epoch **3** (see the LKG caveat in `docs/PURGATORIO-0.1.1.md`)
- minimum security epoch: **3**
- authoritative profile: `include/peregrinus/release_profile.h`

## Remaining Muro stages completed

- **0.3 — e1000 RX Sandbox:** polling RX ring, DMA32, MMIO/BAR ownership only behind the QEMU gate.
- **0.4 — e1000 TX + ARP:** bounded TX ring and ARP reply path.
- **0.5 — Minimal IPv4 control plane:** static QEMU address and ICMP echo only; no sockets/DHCP.
- **0.6 — Stateful Reply Guard:** fixed 32-flow reverse-path table for TCP/UDP replies.
- **0.7 — Burst Guard:** fixed 16-source event-window limiter; it is intentionally not advertised as packets/second.
- **0.8 — Datapath audit:** one 128-entry final-decision ring plus allow/deny counters.
- **0.9 — Adversarial hardening:** randomized parser/ring tests, SAFE-vs-live ELF isolation checks, and corrected negative shell assertions.
- **1.0 — Stable checkpoint:** production/live profiles separated and all earlier Noé/Jó/Muro host regressions retained.

## Explicit non-goals in 1.0

No IPv6, VLAN, DHCP, NAT, socket API, TCP implementation, interrupt-driven NIC path, physical-NIC enablement, or production hardware watchdog arming is claimed. The live e1000 profile is for QEMU qualification only (runtime-tested since 0.1.1, see below).

See `docs/MURO-1.0.md`, `docs/CAPABILITY-MATRIX.md`, `docs/ARCHITECTURE.md`, and the historical `docs/LEAN-AUDIT.md`.


## 1.0.1 hardening

- manual boot-request tooling now derives generation/epoch from the authoritative release profile;
- epoch-config generator no longer has historical defaults;
- incompatible live qualification profiles are compile-time rejected;
- e1000 qualification refuses bare-metal execution and requires a QEMU/KVM-style hypervisor;
- LKG advanced to the real Muro 1.0 gen21 image, same security epoch.


## Purgatório 0.1

Purgatório 0.1 adds a bounded in-memory component quarantine/admission gate. It does **not** claim antivirus scanning, filesystem relocation, process isolation, or persistent quarantine. Components can be denied by identity after an integrity/signature/policy/malformed/manual-hold event. The registry is fixed at 32 entries; saturation fails closed for optional component admission. There is deliberately no runtime release/unquarantine API in this phase. Since 0.1.1 admission is an **allowlist** (unknown or wrong-digest components are denied). No loader calls the gate yet.


## Purgatório 0.1.1 — runtime-qualified correctness release

Before 0.1.1 the profiles had only ever been tested host-side. Booting them revealed that the
e1000 profile halted before starting its datapath and the recovery-live profile always
panicked. 0.1.1 fixes those, an AHCI timeout that was reported as success, and hardens the
kernel. Every claim below is exercised by `make check` or by `scripts/qemu-qualify.sh`
(which runs in CI). Details: `docs/PURGATORIO-0.1.1.md`.

## Build, test, boot

```sh
make                       # SAFE kernel
make check                 # host regression (must exit 0)
./scripts/fetch-limine.sh  # pinned, sha256-verified Limine 12.9.1 -> third_party/limine-dist/
./scripts/qemu-qualify.sh  # boots every profile under QEMU (BIOS + UEFI) and checks the serial log
./scripts/make-iso.sh safe # hybrid BIOS/UEFI ISO, also bootable from a USB stick (dd)
./scripts/run-qemu.sh safe # interactive run, serial on stdio
FUZZ_SECONDS=60 ./tests/fuzz.sh   # libFuzzer + ASan/UBSan over all untrusted-input parsers
```

The boot log is shown on screen (framebuffer text console) as well as on COM1. After boot a small
shell (`peregrinus>`; type `ajuda`) reads the PS/2 keyboard (ABNT2 by default) and COM1, and runs a
bounded HolyC subset (`hc`, see `docs/HOLYC.md`). An experimental `llm-local` profile runs a
small language model from the USB stick (`conversa`, see `docs/IA-LOCAL.md`). The `ia-ponte` profile
asks a larger model running on the GPU of a Linux host through the serial port (`pergunte`; Linux
side and Tesla P100/CUDA setup in `docs/IA-GPU.md`; the GPU part is NOT RUN).

Running it in a virtual machine next to your desktop OS (QEMU tested; VirtualBox and Hyper-V
NOT RUN): see `docs/VIRTUAL-MACHINE.md`.
