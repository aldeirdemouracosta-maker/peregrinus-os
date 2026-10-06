#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/host-cxx.sh"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
host_cxx "$ROOT/tests/network_parser_test.cpp" \
  "$ROOT"/kernel/net/{ethernet_ipv4,datapath,stateful_guard,burst_guard,nic_probe}.cpp \
  "$ROOT"/kernel/security/firewall.cpp "$ROOT"/kernel/pci/pci.cpp "$ROOT"/kernel/console/{serial,format}.cpp -o "$TMP/t"
"$TMP/t"
echo 'PASS: Ethernet/IPv4 parser, NIC classifier and the single bounded datapath audit ring.'
