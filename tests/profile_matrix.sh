#!/usr/bin/env bash
# Build-profile matrix: each profile's flags are asserted at compile time, and forbidden
# combinations must fail to compile.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/host-cxx.sh"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
T="$ROOT/tests/profile_matrix_test.cpp"
host_cxx -DEXPECT_SAFE "$T" -o "$TMP/a"
host_cxx -DEXPECT_E1000 -DPEREGRINUS_QEMU_E1000_SANDBOX=1 "$T" -o "$TMP/a"
host_cxx -DEXPECT_RECOVERY_LIVE -DPEREGRINUS_RECOVERY_COMMIT_LIVE=1 "$T" -o "$TMP/a"
host_cxx -DEXPECT_RECOVERY_COMMIT_TEST -DPEREGRINUS_QEMU_RECOVERY_COMMIT_TEST=1 "$T" -o "$TMP/a"
for bad in PEREGRINUS_RECOVERY_COMMIT_LIVE PEREGRINUS_QEMU_RECOVERY_COMMIT_TEST PEREGRINUS_QEMU_DMA_TEST PEREGRINUS_QEMU_RECOVERY_JOURNAL_TEST; do
  if host_cxx -DEXPECT_E1000 -DPEREGRINUS_QEMU_E1000_SANDBOX=1 -D$bad=1 "$T" -o "$TMP/a" 2>/dev/null; then
    echo "FAIL: e1000 + $bad compiled; live profiles must be mutually exclusive" >&2; exit 1
  fi
done
echo 'PASS: profile matrix (SAFE / e1000 / recovery-live / QEMU commit) and mutual exclusion of live profiles.'
