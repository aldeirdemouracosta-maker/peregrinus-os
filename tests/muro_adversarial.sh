#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/host-cxx.sh"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
host_cxx "$ROOT/tests/muro_adversarial_test.cpp" "$ROOT/kernel/net/datapath.cpp" "$ROOT/kernel/net/stateful_guard.cpp" "$ROOT/kernel/net/burst_guard.cpp" "$ROOT/kernel/net/e1000_model.cpp" "$ROOT/kernel/net/ethernet_ipv4.cpp" "$ROOT/kernel/security/firewall.cpp" -o "$TMP/t"
"$TMP/t"
