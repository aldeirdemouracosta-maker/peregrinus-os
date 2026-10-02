#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BASE="${1:-${PEREGRINUS_TPM_COUNTER_BASE:-}}"
[[ -n "$BASE" ]] || { echo "Usage: $0 <current-absolute-tpm-counter>" >&2; exit 2; }
exec "$ROOT/scripts/prepare-epoch-transition.sh" "$BASE"
