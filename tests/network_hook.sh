#!/usr/bin/env sh
set -eu
c++ -std=c++23 -Wall -Wextra -Werror -Iinclude -Ikernel \
  tests/network_hook_test.cpp \
  kernel/net/ethernet_ipv4.cpp kernel/net/hook.cpp kernel/net/nic_probe.cpp \
  kernel/security/firewall.cpp kernel/pci/pci.cpp kernel/console/serial.cpp kernel/console/format.cpp \
  -o /tmp/peregrinus-network-hook-test
/tmp/peregrinus-network-hook-test
echo 'PASS: Muro 1.0.1 Ethernet/IPv4 parser, ingress/egress hook, audit ring and NIC classifier.'
