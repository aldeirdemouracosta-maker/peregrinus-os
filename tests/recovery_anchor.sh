#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/host-cxx.sh"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
host_cxx "$ROOT/tests/recovery_anchor_test.cpp" "$ROOT/kernel/storage/recovery_anchor.cpp" "$ROOT/kernel/storage/gpt.cpp" -o "$TMP/t"
"$TMP/t"
echo 'PASS: recovery anchor A/B parsing and selection.'
