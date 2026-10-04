#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/host-cxx.sh"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
host_cxx -DPEREGRINUS_QEMU_RECOVERY_COMMIT_TEST=1 -pthread "$ROOT/tests/ahci_timeout_test.cpp" "$ROOT/kernel/storage/ahci.cpp" -o "$TMP/t"
"$TMP/t"
echo 'PASS: AHCI command timeout/error is reported as failure and poisons the controller (fail-closed).'
