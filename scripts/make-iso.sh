#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
PROFILE=${1:-safe}
command -v xorriso >/dev/null 2>&1 || { echo "ERROR: xorriso is required" >&2; exit 1; }
[ -x third_party/limine/limine ] || { echo "ERROR: Limine not built. Run scripts/fetch-limine.sh then make -C third_party/limine" >&2; exit 1; }
for f in limine-bios.sys limine-bios-cd.bin limine-uefi-cd.bin; do
  [ -f "third_party/limine/$f" ] || { echo "ERROR: missing third_party/limine/$f" >&2; exit 1; }
done
case "$PROFILE" in
  safe) make; BUILD=build; NAME=peregrinus-muro-1.0-safe.iso ;;
  dma-test) make dma-test-kernel; BUILD=build-qemu-dma; NAME=peregrinus-muro-1.0-qemu-dma.iso ;;
  e1000) make qemu-e1000-sandbox; BUILD=build-qemu-e1000; NAME=peregrinus-muro-1.0-e1000-sandbox.iso ;;
  *) echo "usage: $0 [safe|dma-test|e1000]" >&2; exit 2 ;;
esac
rm -rf "$BUILD/iso_root"
mkdir -p "$BUILD/iso_root/boot/limine"
cp "$BUILD/peregrinus.elf" "$BUILD/iso_root/boot/peregrinus.elf"
cp limine.conf "$BUILD/iso_root/boot/limine/limine.conf"
cp third_party/limine/limine-bios.sys third_party/limine/limine-bios-cd.bin third_party/limine/limine-uefi-cd.bin "$BUILD/iso_root/boot/limine/"
xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
  --efi-boot boot/limine/limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label \
  "$BUILD/iso_root" -o "$BUILD/$NAME"
third_party/limine/limine bios-install "$BUILD/$NAME"
sha256sum "$BUILD/$NAME"
