#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/host-cxx.sh"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
host_cxx "$ROOT/tests/identify_parser_test.cpp" "$ROOT/kernel/storage/identify.cpp" -o "$TMP/t"
"$TMP/t"
echo 'PASS: ATA IDENTIFY parser host self-test.'
