#!/usr/bin/env bash
set -euo pipefail
CXX=${CXX:-clang++}
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
$CXX -std=c++23 -O2 -Ikernel tests/sha256_test.cpp kernel/security/sha256.cpp -o "$TMP/t"
"$TMP/t"
echo 'PASS: SHA-256 known-answer tests.'
