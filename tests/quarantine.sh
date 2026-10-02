#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
clang++ -std=c++23 -Wall -Wextra -Werror -I"$ROOT" "$ROOT/tests/quarantine_test.cpp" "$ROOT/kernel/security/quarantine.cpp" -o "$TMP/q"
"$TMP/q"
echo 'PASS: Purgatorio bounded component quarantine/admission gate is fail-closed on saturation.'
