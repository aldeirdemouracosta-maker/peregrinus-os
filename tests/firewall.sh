#!/usr/bin/env bash
set -euo pipefail
CXX=${CXX:-clang++}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
"$CXX" -std=c++23 -O2 -Wall -Wextra -Werror -Ikernel tests/firewall_test.cpp kernel/security/firewall.cpp -o "$TMP/firewall-test"
"$TMP/firewall-test"
echo 'PASS: Muro 1.0.1 default-deny allowlist and bounded audit ring.'
