#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/host-cxx.sh"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
host_cxx "$ROOT/tests/sha256_test.cpp" "$ROOT/kernel/security/sha256.cpp" -o "$TMP/t"
"$TMP/t"
echo 'PASS: SHA-256 known-answer tests.'
