#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/host-cxx.sh"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
host_cc "$ROOT/tests/tpm_monotonic_test.c" -o "$TMP/t"
"$TMP/t"
if grep -R --line-number -E 'TPM_CC_NV_Increment|TPM2_NV_Increment|0x00000134' "$ROOT/bootctl" "$ROOT/kernel" >/dev/null; then echo 'FAIL: TPM mutation command found in boot path' >&2; exit 1; fi
printf '%s\n' 'PASS: boot path contains no TPM NV increment/provisioning command'
