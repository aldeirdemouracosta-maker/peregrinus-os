#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
printf 'current-slot\n' > "$TMP/current.elf"
printf 'lkg-slot\n' > "$TMP/lkg.elf"
mkdir -p "$TMP/root/boot"
cp "$TMP/current.elf" "$TMP/root/boot/peregrinus-current.elf"
cp "$TMP/lkg.elf" "$TMP/root/boot/peregrinus-lkg.elf"

# Cross-epoch transition: old LKG must disappear after commit.
"$ROOT/scripts/generate-epoch-configs.py" --current "$TMP/current.elf" --lkg "$TMP/lkg.elf" --staged-output "$TMP/staged-old.conf" --committed-output "$TMP/committed-old.conf" --current-generation 6 --lkg-generation 5 --current-epoch 3 --lkg-epoch 2 >/dev/null
"$ROOT/scripts/verify-epoch-configs.py" --staged "$TMP/staged-old.conf" --committed "$TMP/committed-old.conf" --root "$TMP/root"
grep -q '^//LKG$' "$TMP/staged-old.conf"
if grep -q '^//LKG$' "$TMP/committed-old.conf"; then echo 'FAIL: old-epoch LKG survived committed config' >&2; exit 1; fi

# Stable same-epoch runtime: authenticated LKG remains available.
"$ROOT/scripts/generate-epoch-configs.py" --current "$TMP/current.elf" --lkg "$TMP/lkg.elf" --staged-output "$TMP/staged-same.conf" --committed-output "$TMP/committed-same.conf" --current-generation 7 --lkg-generation 6 --current-epoch 3 --lkg-epoch 3 >/dev/null
"$ROOT/scripts/verify-epoch-configs.py" --staged "$TMP/staged-same.conf" --committed "$TMP/committed-same.conf" --root "$TMP/root" --committed-lkg
grep -q '^//LKG$' "$TMP/committed-same.conf"
echo 'PASS: committed config removes old-epoch LKG but preserves authenticated same-epoch LKG.'
