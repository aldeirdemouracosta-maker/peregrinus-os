#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/host-cxx.sh"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
host_cxx "$ROOT/tests/shell_test.cpp" "$ROOT/kernel/input/keyboard.cpp" "$ROOT/kernel/shell/shell.cpp" "$ROOT/kernel/shell/holyc.cpp" -o "$TMP/t"
"$TMP/t"
echo 'PASS: PS/2 set-1 decoder, bounded line editor and shell commands.'
