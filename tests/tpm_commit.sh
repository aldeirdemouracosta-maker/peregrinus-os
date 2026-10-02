#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CC="${CC:-cc}"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
"$CC" -std=c11 -Wall -Wextra -Werror -I"$ROOT/include" "$ROOT/tests/tpm_commit_test.c" -o "$TMP/tpm_commit_test"
"$TMP/tpm_commit_test"
