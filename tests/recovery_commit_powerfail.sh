#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/host-cxx.sh"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
host_cxx "$ROOT/tests/recovery_commit_powerfail_test.cpp" -o "$TMP/t"
"$TMP/t"
