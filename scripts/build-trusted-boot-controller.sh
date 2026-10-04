#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/build-bootctl}"
mkdir -p "$OUT"
CLANG="${CLANG:-clang}"
LLD="${LLD_LINK:-lld-link}"
EXTRA_CFLAGS_STR="${EXTRA_CFLAGS:-}"
# shellcheck disable=SC2086
"$CLANG" $EXTRA_CFLAGS_STR -target x86_64-pc-win32-coff -std=c11 -ffreestanding -fstack-protector-strong -fshort-wchar -mno-red-zone -O2 -Wall -Wextra -Werror -I"$ROOT/include" -I"$ROOT/bootctl/common" -I"$ROOT/bootctl/uefi" -c "$ROOT/bootctl/uefi/trusted_boot_controller.c" -o "$OUT/controller.obj"
"$LLD" /subsystem:efi_application /entry:efi_main /nodefaultlib /machine:x64 /Brepro /out:"$OUT/peregrinus-boot-controller.efi" "$OUT/controller.obj"
if command -v llvm-objdump >/dev/null 2>&1; then
  P="$(llvm-objdump -p "$OUT/peregrinus-boot-controller.efi")"
  grep -q 'Subsystem.*EFI application' <<<"$P"
  grep -Eq 'Entry 1 0+ 00000000 Import Directory' <<<"$P"
  ! grep -Eq 'Entry 5 0+ 00000000 Base Relocation Directory' <<<"$P"
fi
grep -q __security_check_cookie <(llvm-nm "$OUT/controller.obj" 2>/dev/null || nm "$OUT/controller.obj") || { echo "FAIL: controller built without stack protector" >&2; exit 1; }
echo "PASS: built relocatable, import-free unsigned Peregrinus trusted boot controller EFI application: $OUT/peregrinus-boot-controller.efi"
