#!/usr/bin/env bash
set -euo pipefail
ELF="${1:-build/peregrinus.elf}"
[ -f "$ELF" ] || { echo "FAIL: missing $ELF"; exit 1; }
file "$ELF" | grep -q 'ELF 64-bit LSB executable, x86-64' || { echo 'FAIL: wrong ELF architecture'; exit 1; }
for sec in .limine_requests_start .limine_requests .limine_requests_end .text .rodata .data .bss; do
  readelf -S --wide "$ELF" | grep -q " $sec " || { echo "FAIL: missing section $sec"; exit 1; }
done
readelf -h "$ELF" | grep -q 'Entry point address:' || { echo 'FAIL: no entry point'; exit 1; }
echo 'PASS: ELF structure and Limine request sections present.'
