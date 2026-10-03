#!/usr/bin/env bash
# Provisions and verifies the Peregrinus TPM monotonic counter (NV index
# 0x0180F050) that the trusted boot controller reads as its epoch anchor.
#
#   provision-tpm-counter.sh            verify only (default, changes nothing)
#   provision-tpm-counter.sh --define   define the index if it does not exist
#   provision-tpm-counter.sh --define --advance-to N
#                                       also increment up to N (TEST TPMs only:
#                                       NV counters can never go back down)
#
# Required attributes, as the controller checks them: counter type, OWNERWRITE,
# AUTHREAD with an empty authValue, no AUTHWRITE, 8 bytes. NO_DA is required
# here too: with an empty authValue dictionary-attack protection adds nothing,
# but after an unclean shutdown it makes NV_Read fail with TPM_RC_LOCKOUT, which
# silently drops the boot to the controller's CURRENT-only path.
#
# The TPM is selected by tpm2-tools' usual TPM2TOOLS_TCTI (e.g. device:/dev/tpmrm0).
set -euo pipefail
IDX="${PEREGRINUS_TPM_NV_INDEX:-0x0180F050}"
DEFINE=0; ADVANCE=""
while [ $# -gt 0 ]; do
  case "$1" in
    --define) DEFINE=1 ;;
    --advance-to) ADVANCE="${2:?--advance-to needs a value}"; shift ;;
    *) echo "usage: $0 [--define [--advance-to N]]" >&2; exit 2 ;;
  esac
  shift
done
[ -z "$ADVANCE" ] || [ "$DEFINE" = 1 ] || { echo "ERROR: --advance-to requires --define" >&2; exit 2; }
for t in tpm2_nvreadpublic tpm2_nvdefine tpm2_nvincrement tpm2_nvread; do
  command -v "$t" >/dev/null || { echo "ERROR: tpm2-tools not installed ($t)" >&2; exit 2; }
done

# TPMA_NV bits (TPM 2.0 Part 2): type field 0xF0 (counter = 0x10), OWNERWRITE
# 0x2, AUTHWRITE 0x4, OWNERREAD 0x20000, AUTHREAD 0x40000, NO_DA 0x2000000,
# WRITTEN 0x20000000.
attributes() { tpm2_nvreadpublic "$IDX" 2>/dev/null | awk '/value: 0x/ && prev ~ /friendly/ {print $2} {prev=$1}' | tail -1; }
read_counter() {
  tpm2_nvread "$IDX" -C "$IDX" -s 8 2>/dev/null | python3 -c 'import sys;b=sys.stdin.buffer.read();print(int.from_bytes(b,"big") if len(b)==8 else "")'
}

if ! tpm2_nvreadpublic "$IDX" >/dev/null 2>&1; then
  if [ "$DEFINE" != 1 ]; then echo "FAIL-CLOSED: NV index $IDX is not defined (run with --define)"; exit 3; fi
  tpm2_nvdefine "$IDX" -C o -s 8 -a "nt=counter|ownerwrite|authread|ownerread|no_da" >/dev/null
  echo "Defined NV counter $IDX"
fi

A=$(attributes)
[ -n "$A" ] || { echo "FAIL-CLOSED: cannot read attributes of $IDX" >&2; exit 3; }
python3 - "$A" <<'PY' || exit 3
import sys
a=int(sys.argv[1],16)
checks=[((a>>4)&0xF==1,'type is counter'),(a&0x2,'OWNERWRITE'),(not a&0x4,'no AUTHWRITE'),
        (a&0x40000,'AUTHREAD'),(a&0x2000000,'NO_DA')]
bad=[n for ok,n in checks if not ok]
if bad:
    print(f'FAIL-CLOSED: NV index attributes 0x{a:08x} violate: {", ".join(bad)}')
    print('Redefine the index (tpm2_nvundefine, then --define); a counter cannot be fixed in place.')
    sys.exit(3)
PY

if [ "$DEFINE" = 1 ] && [ -z "$(read_counter)" ]; then
  tpm2_nvincrement "$IDX" -C o   # first increment sets WRITTEN
fi
CUR=$(read_counter)
[ -n "$CUR" ] || { echo "FAIL-CLOSED: index defined but never incremented (run with --define)"; exit 3; }
if [ -n "$ADVANCE" ]; then
  [ "$CUR" -le "$ADVANCE" ] || { echo "FAIL-CLOSED: counter already at $CUR > $ADVANCE; NV counters cannot decrease" >&2; exit 4; }
  while [ "$CUR" -lt "$ADVANCE" ]; do tpm2_nvincrement "$IDX" -C o; CUR=$(read_counter); done
fi
echo "PASS: NV counter $IDX attributes $(attributes), value $CUR"
