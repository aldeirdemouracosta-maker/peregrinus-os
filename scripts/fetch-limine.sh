#!/usr/bin/env sh
# Download, verify and build the pinned Limine release used for bootable images.
# The kernel only vendors the protocol subset in third_party/limine/limine_min.h; the
# bootloader binaries are built into third_party/limine-dist/ (ignored by git).
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
VERSION=12.9.1
SHA256=c1096fdd506487fbd92c113baa9e153a9973cf766483cc3928e13fd29b976b32
URL="https://github.com/limine-bootloader/limine/releases/download/v$VERSION/limine-$VERSION.tar.xz"
DIST=third_party/limine-dist
if [ -x "$DIST/bin/limine" ] && [ "$(cat "$DIST/VERSION" 2>/dev/null)" = "$VERSION" ]; then
  echo "Limine $VERSION already built in $DIST"; exit 0
fi
for t in curl tar make cc nasm mtools; do
  [ "$t" = mtools ] && t=mcopy
  command -v "$t" >/dev/null 2>&1 || { echo "ERROR: $t is required to build Limine" >&2; exit 1; }
done
rm -rf "$DIST"; mkdir -p "$DIST"
curl -fsSL "$URL" -o "$DIST/limine.tar.xz"
echo "$SHA256  $DIST/limine.tar.xz" | sha256sum -c --quiet
tar -C "$DIST" --strip-components=1 -xf "$DIST/limine.tar.xz"
(cd "$DIST" && ./configure --enable-bios --enable-bios-cd --enable-uefi-x86-64 --enable-uefi-cd >/dev/null && make -j"$(nproc 2>/dev/null || echo 2)" >/dev/null)
echo "$VERSION" > "$DIST/VERSION"
echo "Limine $VERSION verified (sha256) and built in $DIST/bin. Do not replace with an unpinned build in release images."
