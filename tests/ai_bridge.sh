#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/host-cxx.sh"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
host_cxx "$ROOT/tests/ai_bridge_test.cpp" "$ROOT/kernel/shell/ai_bridge.cpp" -o "$TMP/t"
"$TMP/t"
echo 'PASS: serial AI bridge protocol (framing, stale/malformed answers, sanitizing, truncation, Esc, timeout).'
