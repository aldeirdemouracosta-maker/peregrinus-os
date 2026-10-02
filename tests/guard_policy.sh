#!/usr/bin/env bash
set -euo pipefail
CXX=${CXX:-clang++}; TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
$CXX -std=c++23 -O2 -Ikernel tests/guard_policy_test.cpp kernel/security/guard_policy.cpp -o "$TMP/t"
"$TMP/t"; echo 'PASS: Peregrinus Guard fail-closed policy.'
