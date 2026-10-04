#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
P="$ROOT/scripts/release-profile.py"
[[ "$($P current_generation)" == 24 ]]
[[ "$($P lkg_generation)" == 22 ]]
[[ "$($P current_epoch)" == 3 ]]
[[ "$($P lkg_epoch)" == 3 ]]
[[ "$($P min_epoch)" == 3 ]]
grep -q 'PEREGRINUS_RELEASE_CURRENT_GENERATION' "$ROOT/kernel/security/root_trust.hpp"
grep -q 'PEREGRINUS_RELEASE_CURRENT_GENERATION' "$ROOT/bootctl/uefi/trusted_boot_controller.c"
grep -q 'PEREGRINUS_RELEASE_MIN_SECURITY_EPOCH' "$ROOT/bootctl/common/boot_request.h"
echo 'PASS: one authoritative release profile feeds kernel, UEFI controller and tooling.'
