#!/usr/bin/env bash
set -euo pipefail
ELF=${1:-build/peregrinus.elf}
OBJCOPY=$(command -v llvm-objcopy || command -v objcopy)
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
cp "$ELF" "$TMP/tampered.elf"
"$OBJCOPY" --dump-section .text="$TMP/text.bin" "$TMP/tampered.elf"
python3 - "$TMP/text.bin" <<'PY'
import sys
p=sys.argv[1]; b=bytearray(open(p,'rb').read())
if not b: raise SystemExit('empty .text')
b[len(b)//2] ^= 0x01
open(p,'wb').write(b)
PY
"$OBJCOPY" --update-section .text="$TMP/text.bin" "$TMP/tampered.elf"
if ./tests/integrity_seal.sh "$TMP/tampered.elf" >/dev/null 2>&1; then
  echo 'FAIL: tampered .text incorrectly accepted' >&2
  exit 1
fi
echo 'PASS: one-byte .text tamper is detected by the embedded manifest.'
