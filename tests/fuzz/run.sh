#!/usr/bin/env bash
# Builds and runs the network fuzzer. FUZZ_SECONDS (default 60) bounds the run;
# the seed corpus is regenerated from the parser self-test frames each time.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${FUZZ_OUT:-$ROOT/build-fuzz}"
SECONDS_BUDGET="${FUZZ_SECONDS:-60}"
mkdir -p "$OUT/corpus"
clang++ -std=c++23 -O1 -g -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined \
  -I"$ROOT" -I"$ROOT/include" -I"$ROOT/kernel" \
  "$ROOT/tests/fuzz/net_fuzz.cpp" \
  "$ROOT/kernel/net/arp.cpp" "$ROOT/kernel/net/icmp.cpp" "$ROOT/kernel/net/datapath.cpp" \
  "$ROOT/kernel/net/stateful_guard.cpp" "$ROOT/kernel/net/burst_guard.cpp" \
  "$ROOT/kernel/net/ethernet_ipv4.cpp" "$ROOT/kernel/net/e1000_model.cpp" "$ROOT/kernel/security/firewall.cpp" \
  -o "$OUT/net_fuzz"
python3 "$ROOT/tests/fuzz/seeds.py" "$OUT/corpus"
"$OUT/net_fuzz" "$OUT/corpus" -max_total_time="$SECONDS_BUDGET" -max_len=1600 -print_final_stats=1 2>&1 | tail -15
echo "PASS: network fuzzer ran ${SECONDS_BUDGET}s without crashes or invariant failures"
