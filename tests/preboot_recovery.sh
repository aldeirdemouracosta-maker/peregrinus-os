#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/include" -I"$ROOT/bootctl/common" "$ROOT/tests/preboot_recovery_test.c" -o "$TMP/t"
"$TMP/t"
