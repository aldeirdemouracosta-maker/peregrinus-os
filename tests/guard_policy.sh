#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/host-cxx.sh"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
host_cxx "$ROOT/tests/guard_policy_test.cpp" "$ROOT/kernel/security/guard_policy.cpp" -o "$TMP/t"
"$TMP/t"
echo 'PASS: Peregrinus Guard fail-closed policy (incl. passive builds).'
