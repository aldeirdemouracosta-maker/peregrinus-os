#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/bin"
cat > "$TMP/plan" <<PLAN
PEREGRINUS_EPOCH_COMMIT_PLAN=1
TPM_NV_INDEX=0x0180F050
TPM_COUNTER_BASE=41
TPM_COUNTER_NEXT=42
PLAN
printf '41\n' > "$TMP/state"
printf 'owner-secret\n' > "$TMP/auth"
cat > "$TMP/bin/tpm2_nvread" <<'MOCK'
#!/usr/bin/env bash
set -euo pipefail
OUT=""
while (($#)); do case "$1" in -o) OUT="$2"; shift 2;; *) shift;; esac; done
v=$(cat "$PGR_TEST_STATE")
python3 - "$OUT" "$v" <<'PY'
import pathlib,sys
pathlib.Path(sys.argv[1]).write_bytes(int(sys.argv[2]).to_bytes(8,'big'))
PY
MOCK
cat > "$TMP/bin/tpm2_nvincrement" <<'MOCK'
#!/usr/bin/env bash
set -euo pipefail
v=$(cat "$PGR_TEST_STATE"); printf '%s\n' "$((v+1))" > "$PGR_TEST_STATE"
MOCK
chmod +x "$TMP/bin/tpm2_nvread" "$TMP/bin/tpm2_nvincrement"
export PATH="$TMP/bin:$PATH" PGR_TEST_STATE="$TMP/state" PEREGRINUS_EPOCH_PLAN="$TMP/plan" PEREGRINUS_TPM_OWNER_AUTH_FILE="$TMP/auth"
"$ROOT/scripts/commit-tpm-epoch.sh" >/dev/null
[[ $(cat "$TMP/state") == 41 ]]
"$ROOT/scripts/commit-tpm-epoch.sh" --execute >/dev/null
[[ $(cat "$TMP/state") == 42 ]]
"$ROOT/scripts/commit-tpm-epoch.sh" --execute >/dev/null
[[ $(cat "$TMP/state") == 42 ]]
printf '40\n' > "$TMP/state"
if "$ROOT/scripts/commit-tpm-epoch.sh" >/dev/null 2>&1; then echo 'FAIL: stale counter accepted' >&2; exit 1; fi
echo 'PASS: epoch commit ceremony is dry-run by default, exactly-once, idempotent after commit, and fail-closed on stale state.'
