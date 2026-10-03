#!/usr/bin/env sh
# Fetches the pinned Limine release tarball, verifies its SHA-256 and builds
# the BIOS/UEFI boot files into third_party/limine-bin (git-ignored).
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
VERSION=12.9.1
SHA256=ee7c498670d0d16c897ecb391cd837cd375081cfd364bd3198db144c92aaccb4
URL="https://github.com/limine-bootloader/limine/releases/download/v$VERSION/limine-$VERSION.tar.gz"
OUT=third_party/limine-bin
if [ -x "$OUT/limine" ] && [ "$(cat "$OUT/VERSION" 2>/dev/null)" = "$VERSION" ]; then
  echo "Limine $VERSION already built in $OUT"; exit 0
fi
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
curl -fsSL -o "$WORK/limine.tar.gz" "$URL"
echo "$SHA256  $WORK/limine.tar.gz" | sha256sum -c --quiet - || { echo "ERROR: Limine tarball SHA-256 mismatch" >&2; exit 1; }
tar -xzf "$WORK/limine.tar.gz" -C "$WORK"
( cd "$WORK/limine-$VERSION" && ./configure --enable-bios --enable-bios-cd --enable-uefi-x86-64 --enable-uefi-cd >/dev/null && make -j"$(nproc)" >/dev/null )
rm -rf "$OUT"; mkdir -p "$OUT"
for f in limine limine-bios.sys limine-bios-cd.bin limine-uefi-cd.bin BOOTX64.EFI; do cp "$WORK/limine-$VERSION/bin/$f" "$OUT/"; done
echo "$VERSION" > "$OUT/VERSION"
printf '%s\n' "Limine pinned to v$VERSION (sha256 $SHA256). Do not replace with an unpinned branch in release builds."
