#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/host-cxx.sh"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
host_cxx "$ROOT/tests/quarantine_test.cpp" "$ROOT/kernel/security/quarantine.cpp" -o "$TMP/t"
"$TMP/t"
echo 'PASS: Purgatorio allowlist admission gate (unknown and wrong-digest components denied; quarantine and saturation fail closed).'
