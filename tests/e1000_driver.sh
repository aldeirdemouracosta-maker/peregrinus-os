#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/host-cxx.sh"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
host_cxx -DPEREGRINUS_QEMU_E1000_SANDBOX=1 -pthread "$ROOT/tests/e1000_driver_test.cpp" "$ROOT/kernel/net/e1000.cpp" "$ROOT/kernel/net/e1000_model.cpp" -o "$TMP/t"
"$TMP/t"
echo 'PASS: e1000 resets before bus mastering and fails closed on TX completion timeout.'
