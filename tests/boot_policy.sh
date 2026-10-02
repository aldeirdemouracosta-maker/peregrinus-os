#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CXX="${CXX:-clang++}"
OUT="${TMPDIR:-/tmp}/peregrinus-boot-policy-test.$$"
trap 'rm -f "$OUT"' EXIT
"$CXX" -std=c++23 -O2 -Wall -Wextra \
  "$ROOT/tests/boot_policy_test.cpp" \
  "$ROOT/kernel/storage/boot_policy.cpp" \
  -o "$OUT"
"$OUT"
