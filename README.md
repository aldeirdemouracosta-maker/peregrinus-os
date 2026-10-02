# Peregrinus OS — Peregrinus Guard — Purgatório 0.1 Admission Gate

Muro 1.0 closes the first firewall/network-foundation cycle on top of the Lean/Hardened 0.2.1 checkpoint. The production **SAFE** kernel remains passive: it parses and enforces policy only on in-memory/test frames and does not own a NIC, map an Ethernet BAR, enable NIC bus mastering, reserve DMA32 for networking, or transmit packets.

A separate **QEMU e1000 qualification profile** contains the live datapath. It is deliberately narrow: Intel 82540EM/e1000 (`8086:100e`), polling only, 8 RX + 8 TX descriptors, static IPv4 `10.0.2.15`, ARP response, ICMP echo response, default-deny IPv4 policy, stateful reply guard for TCP/UDP, event-window burst protection, and bounded audit.

## Release profile

- CURRENT: generation **23**, security epoch **3**
- LKG: Muro 1.0.1 generation **22**, security epoch **3**
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

No IPv6, VLAN, DHCP, NAT, socket API, TCP implementation, interrupt-driven NIC path, physical-NIC enablement, or production hardware watchdog arming is claimed. The live e1000 profile is for QEMU/qualification until runtime testing is performed.

See `docs/MURO-1.0.md`, `docs/CAPABILITY-MATRIX.md`, `docs/ARCHITECTURE.md`, and the historical `docs/LEAN-AUDIT.md`.


## 1.0.1 hardening

- manual boot-request tooling now derives generation/epoch from the authoritative release profile;
- epoch-config generator no longer has historical defaults;
- incompatible live qualification profiles are compile-time rejected;
- e1000 qualification refuses bare-metal execution and requires a QEMU/KVM-style hypervisor;
- LKG advanced to the real Muro 1.0 gen21 image, same security epoch.


## Purgatório 0.1

Purgatório 0.1 adds a bounded in-memory component quarantine/admission gate. It does **not** claim antivirus scanning, filesystem relocation, process isolation, or persistent quarantine. Components can be denied by identity after an integrity/signature/policy/malformed/manual-hold event. The registry is fixed at 32 entries; saturation fails closed for optional component admission. There is deliberately no runtime release/unquarantine API in this phase.
