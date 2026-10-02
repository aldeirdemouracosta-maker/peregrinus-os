#!/usr/bin/env bash
set -euo pipefail
CXX=${CXX:-clang++}; TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
$CXX -std=c++23 -O2 -Ikernel tests/watchdog_test.cpp kernel/security/watchdog.cpp -o "$TMP/t"
"$TMP/t"; echo 'PASS: logical boot watchdog sequencing.'
