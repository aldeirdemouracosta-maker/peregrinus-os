#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/host-cxx.sh"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
host_cxx "$ROOT/tests/acpi_tables_test.cpp" "$ROOT/kernel/acpi/acpi.cpp" -o "$TMP/t"
"$TMP/t"
echo 'PASS: ACPI MADT discovered through XSDT and through the ACPI 1.0 RSDT fallback.'
