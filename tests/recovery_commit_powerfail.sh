#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
${CXX:-g++} -std=c++23 -Wall -Wextra -Werror -I"$ROOT/include" "$ROOT/tests/recovery_commit_powerfail_test.cpp" -o "$TMP/test"
"$TMP/test"
