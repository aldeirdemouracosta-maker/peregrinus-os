# Muro das Lamentações 1.0 — Stable Network Foundation

## Objective

Close the Muro phase without turning Peregrinus into a general-purpose network stack. The firewall remains default-deny and the production build remains incapable of taking NIC ownership.

## Stage 0.3 — e1000 RX Sandbox

- Intel/QEMU 82540EM PCI ID `8086:100e` only.
- BAR0 MMIO mapped through the Peregrinus UC MMIO window.
- PCI memory + bus mastering enabled only in `PEREGRINUS_QEMU_E1000_SANDBOX=1`.
- 8 receive descriptors, 16 bytes each: 128-byte ring.
- 2048-byte receive buffers from DMA32.
- polling only; interrupts masked.
- received frames reach policy before any control-plane handling.

## Stage 0.4 — TX + ARP

- 8 transmit descriptors and fixed DMA32 buffers.
- bounded wait for descriptor completion.
- no scatter/gather, checksum offload or jumbo frames.
- ARP parser accepts Ethernet/IPv4 ARP only.
- qualification service replies only to requests targeting its configured IPv4 address.

## Stage 0.5 — Minimal control plane

The QEMU profile uses static IPv4 `10.0.2.15`. There is no DHCP client. The only IPv4 service is ICMP Echo. Both ingress request and generated egress reply pass explicit allow rules.

## Stage 0.6 — Stateful Reply Guard

A fixed table of 32 flows records TCP/UDP traffic already allowed by explicit policy. A reverse packet can be admitted when it matches a recent flow. For TCP, pure inbound SYN is never accepted as a reply; ACK/RST/SYN+ACK semantics are required. Expiry is measured in datapath event sequence, not wall time.

## Stage 0.7 — Burst Guard

A fixed 16-source table limits new inbound flow candidates to 8 admissions per 64 datapath events. This is a deterministic anti-burst mechanism; it is not a clock-based rate limiter.

## Stage 0.8 — Final-decision audit

The final datapath owns a 128-entry audit ring recording parse rejection, explicit allow, stateful reply, default deny or burst rejection. Firewall and parser development audit paths remain available for host tests, but live qualification decisions use the datapath audit.

## Stage 0.9 — Hardening

- deterministic adversarial/random frame tests under an empty default-deny ruleset;
- randomized receive-descriptor validation tests;
- shell tests changed from ambiguous `! grep` assertions to explicit failure branches;
- SAFE ELF inspected to prove absence of e1000 ownership/service and DMA32 allocator symbols;
- live qualification ELF inspected separately.

## Stage 1.0 — Closure

Muro 1.0 baseline was CURRENT generation 21 / epoch 3 with historical LKG generation 12 / epoch 3. Muro 1.0.1 advances LKG to the real Muro 1.0 generation 21 image. No security-epoch transition is performed.

The SAFE build is the distributable default. The e1000 profile is a qualification artifact. Update: QEMU/OVMF execution now succeeds and is checked in CI (`docs/QEMU-BOOT-QUALIFICATION.md`); physical hardware remains untested.
