#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CXX=${CXX:-c++}
OUT=$(mktemp)
trap 'rm -f "$OUT"' EXIT
"$CXX" -std=c++23 -I"$ROOT/kernel" "$ROOT/tests/gpt_redundancy_test.cpp" "$ROOT/kernel/storage/gpt.cpp" -o "$OUT"
"$OUT"
