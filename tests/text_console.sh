#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/host-cxx.sh"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
host_cxx "$ROOT/tests/text_console_test.cpp" "$ROOT/kernel/console/text_console.cpp" "$ROOT/kernel/console/framebuffer.cpp" -o "$TMP/t"
"$TMP/t"
echo 'PASS: framebuffer text console (glyphs, wrap, scroll, UTF-8, bounded grid, unusable framebuffers refused).'
