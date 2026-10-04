#!/usr/bin/env bash
set -euo pipefail
ELF="${1:-build/peregrinus.elf}"
[[ -f "$ELF" ]]
SYMS="$(nm -C "$ELF")"
for s in 'e1000::Driver::init_qemu_sandbox' 'sandbox::Service::init' 'memory::alloc_dma32_page'; do
  if grep -Fq "$s" <<<"$SYMS"; then echo "FAIL: SAFE ELF contains live capability: $s" >&2; exit 1; fi
done
# Hardening present in the final binary, not just in the build flags.
for s in '__stack_chk_guard' '__stack_chk_fail' 'peregrinus_switch_stack'; do
  grep -Fq "$s" <<<"$SYMS" || { echo "FAIL: SAFE ELF lacks hardening symbol: $s" >&2; exit 1; }
done
if command -v llvm-objdump >/dev/null 2>&1 && llvm-objdump -d "$ELF" | grep -q '%fs:'; then
  echo 'FAIL: stack protector uses %fs (TLS) but the kernel has no TLS segment' >&2; exit 1
fi
echo 'PASS: SAFE ELF contains no e1000 ownership/service or DMA32 allocator; stack protector and guard-page stack are linked in.'
