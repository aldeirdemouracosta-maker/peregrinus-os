#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/host-cxx.sh"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
host_cxx "$ROOT/tests/holyc_test.cpp" "$ROOT/kernel/shell/holyc.cpp" -o "$TMP/t"
"$TMP/t"
echo 'PASS: HolyC subset interpreter (semantics, limits and fail-closed errors).'
