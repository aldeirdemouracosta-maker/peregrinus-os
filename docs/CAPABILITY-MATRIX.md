# Capability Matrix (Purgatório 0.1.1)

"QEMU-verified" means `scripts/qemu-qualify.sh` boots that profile and checks the behaviour on the serial log / on the wire.

| Capability | SAFE | QEMU e1000 qualification |
|---|---:|---:|
| Default-deny IPv4 policy | yes | yes |
| Ethernet/IPv4 parser | yes | yes |
| NIC BAR mapping | no | e1000 only |
| NIC bus master / DMA | no | e1000 only |
| RX | no | polling, 8 descriptors |
| TX | no | polling, 8 descriptors |
| NIC interrupts | no | no |
| ARP | no live path | reply for local IP only |
| ICMP echo | no live path | yes |
| TCP sockets/stack | no | no |
| UDP sockets/stack | no | no |
| Stateful reply guard | code/test only | available in datapath |
| Event-window burst guard | code/test only | available in datapath |
| VLAN | denied | denied |
| IPv6 | disabled | disabled |
| DHCP | disabled | disabled |
| NAT | disabled | disabled |
| Jumbo frames | denied | denied |
| IA_RECOVERY generic writes | no | no |
| GPT repair | no | no |
| Physical iTCO arm | no | no |
| Stack protector (canary) | yes | yes |
| Guard-page kernel stack + IST for #DF/NMI/#MC | yes (QEMU-verified) | yes |
| Boot to completion | QEMU-verified (BIOS + UEFI, via controller) | QEMU-verified |
| ARP / ICMP / UDP-deny / burst on the wire | — | QEMU-verified |
| Recovery-live journal commit | — | — (recovery-live profile: QEMU-verified on a disposable disk) |
| Boot log on screen (framebuffer text console) | QEMU-verified (text decoded from a screendump) | yes |
| Shell (ajuda, sobre, status, hw, limpar, reiniciar, parar) via PS/2 keyboard and COM1 | QEMU-verified (sendkey + serial) | yes (serviced with the datapath) |
| Interrupts (PIC + PIT 100 Hz, LAPIC virtual wire), idle hlt | QEMU-verified (BIOS + UEFI; ~2% idle CPU) | yes (NIC still polled) |
| Keyboard layouts ABNT2 (default, dead keys) and US | QEMU-verified | yes |
| Physical hardware | NOT RUN | NOT RUN |
