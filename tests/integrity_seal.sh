#!/usr/bin/env bash
set -euo pipefail
ELF=${1:-build/peregrinus.elf}
OBJCOPY=$(command -v llvm-objcopy || command -v objcopy)
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
"$OBJCOPY" --dump-section .text="$TMP/text.bin" "$ELF"
"$OBJCOPY" --dump-section .peregrinus_integrity="$TMP/manifest.bin" "$ELF"
python3 - "$TMP/text.bin" "$TMP/manifest.bin" <<'PY'
import hashlib,struct,sys
text=open(sys.argv[1],'rb').read(); m=open(sys.argv[2],'rb').read()
assert len(m)==96, len(m)
assert m[:16]==b'PEREGRINUS-GUARD'
ver,size,tsize=struct.unpack_from('<IIQ',m,16)
assert ver==1 and size==96 and tsize==len(text)
want=m[32:64]; got=hashlib.sha256(text).digest()
assert want==got
assert any(want)
print('PASS: embedded Peregrinus Guard manifest matches .text SHA-256.')
PY
