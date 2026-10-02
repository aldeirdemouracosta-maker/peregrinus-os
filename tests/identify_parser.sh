#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="${TMPDIR:-/tmp}/peregrinus-identify-test-$$"
trap 'rm -f "$TMP"' EXIT
${CXX:-c++} -std=c++23 -Wall -Wextra -Werror -I"$ROOT/kernel" \
  "$ROOT/tests/identify_parser_test.cpp" "$ROOT/kernel/storage/identify.cpp" -o "$TMP"
"$TMP"
echo 'PASS: ATA IDENTIFY parser host self-test.'
