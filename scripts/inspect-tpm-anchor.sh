#!/usr/bin/env bash
set -euo pipefail
IDX="${PEREGRINUS_TPM_NV_INDEX:-0x0180F050}"
command -v tpm2_nvreadpublic >/dev/null || { echo "ERROR: tpm2-tools not installed" >&2; exit 2; }
command -v tpm2_nvread >/dev/null || { echo "ERROR: tpm2-tools not installed" >&2; exit 2; }
echo "Peregrinus TPM monotonic anchor: $IDX"
tpm2_nvreadpublic "$IDX"
echo
tmp="$(mktemp)"; trap 'rm -f "$tmp"' EXIT
if ! tpm2_nvread "$IDX" -s 8 -o "$tmp"; then
  echo "Read failed. Peregrinus will treat the anchor as unavailable/untrusted." >&2
  exit 3
fi
python3 - "$tmp" <<'PY'
import pathlib,sys
b=pathlib.Path(sys.argv[1]).read_bytes()
if len(b)!=8: raise SystemExit(f"unexpected counter length: {len(b)}")
print("Counter value:", int.from_bytes(b,"big"))
PY
