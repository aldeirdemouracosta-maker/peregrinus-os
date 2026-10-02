#!/usr/bin/env bash
set -euo pipefail
ELF="${1:-build/peregrinus.elf}"
[[ -f "$ELF" ]]
SYMS="$(nm -C "$ELF")"
for s in 'e1000::Driver::init_qemu_sandbox' 'sandbox::Service::init' 'memory::alloc_dma32_page'; do
  if grep -Fq "$s" <<<"$SYMS"; then echo "FAIL: SAFE ELF contains live capability: $s" >&2; exit 1; fi
done
echo 'PASS: SAFE ELF contains no e1000 ownership/service or DMA32 allocator.'
