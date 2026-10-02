# Muro 1.0 Capability Matrix

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
