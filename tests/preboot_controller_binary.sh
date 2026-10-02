#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
"$ROOT/scripts/build-trusted-boot-controller.sh" "$TMP/safe" >/dev/null
EXTRA_CFLAGS="-DPEREGRINUS_RECOVERY_JOURNAL_LIVE=1 -DPEREGRINUS_TPM_COMMIT_BASE=41ull -DPEREGRINUS_TPM_COMMIT_NEXT=42ull" "$ROOT/scripts/build-trusted-boot-controller.sh" "$TMP/live" >/dev/null
if strings -a "$TMP/safe/peregrinus-boot-controller.efi" | grep -q 'IA_RECOVERY pre-boot journal enabled'; then echo 'FAIL: safe EFI contains live journal backend' >&2; exit 1; fi
strings -a "$TMP/live/peregrinus-boot-controller.efi" | grep -q 'IA_RECOVERY pre-boot journal enabled'
if strings -a "$TMP/safe/peregrinus-boot-controller.efi" | grep -q 'PeregrinusBootSuccess'; then echo 'FAIL: obsolete success variable in safe EFI' >&2; exit 1; fi
if strings -a "$TMP/live/peregrinus-boot-controller.efi" | grep -q 'PeregrinusBootSuccess'; then echo 'FAIL: obsolete success variable in live EFI' >&2; exit 1; fi
if command -v llvm-objdump >/dev/null 2>&1; then
  for f in "$TMP/safe/peregrinus-boot-controller.efi" "$TMP/live/peregrinus-boot-controller.efi"; do
    P="$(llvm-objdump -p "$f")"
    grep -Eq 'Entry 1 0+ 00000000 Import Directory' <<<"$P"
    if grep -Eq 'Entry 5 0+ 00000000 Base Relocation Directory' <<<"$P"; then echo 'FAIL: EFI relocation directory missing' >&2; exit 1; fi
  done
fi
echo 'PASS: safe EFI binary contains no pre-boot disk-journal backend; live test EFI is relocatable/import-free and contains the gated backend.'
