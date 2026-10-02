#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLAN="${PEREGRINUS_EPOCH_PLAN:-$ROOT/build-transition/EPOCH-COMMIT-PLAN.txt}"
EXECUTE=0
[[ "${1:-}" == "--execute" ]] && EXECUTE=1
[[ -f "$PLAN" ]] || { echo "ERROR: missing plan: $PLAN" >&2; exit 2; }
# shellcheck disable=SC1090
source "$PLAN"
IDX="${TPM_NV_INDEX:?}"
BASE="${TPM_COUNTER_BASE:?}"
NEXT="${TPM_COUNTER_NEXT:?}"
command -v tpm2_nvread >/dev/null || { echo 'ERROR: tpm2_nvread not installed' >&2; exit 2; }
command -v tpm2_nvincrement >/dev/null || { echo 'ERROR: tpm2_nvincrement not installed' >&2; exit 2; }
read_counter(){
  local t; t="$(mktemp)"
  if ! tpm2_nvread "$IDX" -s 8 -o "$t" >/dev/null; then rm -f "$t"; return 1; fi
  python3 - "$t" <<'PY'
import pathlib,sys
b=pathlib.Path(sys.argv[1]).read_bytes()
if len(b)!=8: raise SystemExit(2)
print(int.from_bytes(b,'big'))
PY
  rm -f "$t"
}
CUR="$(read_counter)" || { echo 'ERROR: cannot read TPM counter' >&2; exit 3; }
if [[ "$CUR" == "$NEXT" ]]; then
  echo "TPM counter already committed at $NEXT; no increment performed."
  exit 0
fi
[[ "$CUR" == "$BASE" ]] || { echo "FAIL-CLOSED: TPM counter is $CUR; expected staged base $BASE (or committed $NEXT)." >&2; exit 4; }
echo "Preflight PASS: TPM counter is staged at $BASE; target is exactly $NEXT."
echo "This transition is irreversible by design. TPM NV counters can only increment."
if (( EXECUTE == 0 )); then
  echo "DRY RUN ONLY. Re-run with --execute after booting and validating CURRENT generation ${CURRENT_GENERATION:-unknown} / security epoch ${CURRENT_SECURITY_EPOCH:-unknown}."
  exit 0
fi
AUTH_FILE="${PEREGRINUS_TPM_OWNER_AUTH_FILE:-}"
[[ -n "$AUTH_FILE" && -r "$AUTH_FILE" ]] || { echo 'ERROR: set PEREGRINUS_TPM_OWNER_AUTH_FILE to a readable file containing the TPM owner authorization.' >&2; exit 5; }
# Use owner authorization; the provisioned counter is required to have OWNERWRITE and AUTHREAD.
tpm2_nvincrement -C o "$IDX" -P "file:$AUTH_FILE"
AFTER="$(read_counter)" || { echo 'ERROR: increment returned but counter could not be re-read; stop and inspect manually.' >&2; exit 6; }
[[ "$AFTER" == "$NEXT" ]] || { echo "FAIL-CLOSED: expected counter $NEXT after exactly one increment; got $AFTER." >&2; exit 7; }
echo "COMMIT PASS: TPM counter advanced exactly once: $BASE -> $NEXT."
echo "On next boot the controller will chainload the COMMITTED profile; any LKG below the committed minimum epoch is absent."
