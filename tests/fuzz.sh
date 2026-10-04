#!/usr/bin/env bash
# Coverage-guided fuzzing of the untrusted-input parsers with ASan + UBSan.
# FUZZ_SECONDS (default 60) bounds the run; any crash, leak or UB report fails the script.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SECONDS_BUDGET="${FUZZ_SECONDS:-60}"
OUT="${FUZZ_OUT:-$ROOT/build-fuzz}"
mkdir -p "$OUT/corpus"
clang++ -std=c++23 -g -O1 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined \
  -I"$ROOT" -I"$ROOT/include" -I"$ROOT/kernel" "$ROOT/tests/fuzz/fuzz_parsers.cpp" \
  "$ROOT"/kernel/net/{datapath,ethernet_ipv4,stateful_guard,burst_guard,arp,icmp,e1000_model}.cpp \
  "$ROOT"/kernel/security/firewall.cpp "$ROOT"/kernel/storage/{gpt,identify}.cpp "$ROOT"/kernel/shell/holyc.cpp -o "$OUT/fuzz_parsers"
"$OUT/fuzz_parsers" -max_total_time="$SECONDS_BUDGET" -max_len=2048 -print_final_stats=1 "$OUT/corpus" 2>&1 | tail -n 12
echo "PASS: ${SECONDS_BUDGET}s of coverage-guided fuzzing with ASan/UBSan found no defect."
