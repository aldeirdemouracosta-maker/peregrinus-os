#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/peregrinus-muro-adversarial"
clang++ -std=c++23 -O2 -Wall -Wextra -Werror \
  -I"$ROOT" -I"$ROOT/include" -I"$ROOT/kernel" \
  "$ROOT/tests/muro_adversarial_test.cpp" \
  "$ROOT/kernel/net/datapath.cpp" \
  "$ROOT/kernel/net/stateful_guard.cpp" \
  "$ROOT/kernel/net/burst_guard.cpp" \
  "$ROOT/kernel/net/e1000_model.cpp" \
  "$ROOT/kernel/net/ethernet_ipv4.cpp" \
  "$ROOT/kernel/security/firewall.cpp" \
  -o "$OUT"
"$OUT"
rm -f "$OUT"
