#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CXX="${CXX:-clang++}"
mkdir -p "$ROOT/build-tests"
"$CXX" -std=c++23 -Wall -Wextra -Werror -I"$ROOT/include" "$ROOT/tests/recovery_journal_test.cpp" -o "$ROOT/build-tests/recovery_journal_test"
"$ROOT/build-tests/recovery_journal_test"
