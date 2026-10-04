#!/usr/bin/env sh
# Build a hybrid BIOS/UEFI ISO (also bootable when written to a USB stick) for one profile.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
PROFILE=${1:-safe}
LIM=third_party/limine-dist/bin
command -v xorriso >/dev/null 2>&1 || { echo "ERROR: xorriso is required" >&2; exit 1; }
[ -x "$LIM/limine" ] || { echo "ERROR: Limine not built. Run scripts/fetch-limine.sh" >&2; exit 1; }
TAG=$(./scripts/release-profile.py tag | tr 'A-Z' 'a-z')
case "$PROFILE" in
  safe) make; BUILD=build ;;
  dma-test) make dma-test-kernel; BUILD=build-qemu-dma ;;
  recovery-commit-test) make recovery-commit-test-kernel; BUILD=build-recovery-commit-test ;;
  recovery-live) make current-recovery-live; BUILD=build-current-recovery-live ;;
  e1000) make qemu-e1000-sandbox; BUILD=build-qemu-e1000 ;;
  double-fault-test) make double-fault-test-kernel; BUILD=build-double-fault-test ;;
  *) echo "usage: $0 [safe|dma-test|recovery-commit-test|recovery-live|e1000|double-fault-test]" >&2; exit 2 ;;
esac
NAME="peregrinus-$TAG-$PROFILE.iso"
rm -rf "$BUILD/iso_root"
mkdir -p "$BUILD/iso_root/boot/limine" "$BUILD/iso_root/EFI/BOOT"
cp "$BUILD/peregrinus.elf" "$BUILD/iso_root/boot/peregrinus.elf"
cp limine.conf "$BUILD/iso_root/boot/limine/limine.conf"
cp "$LIM/limine-bios.sys" "$LIM/limine-bios-cd.bin" "$LIM/limine-uefi-cd.bin" "$BUILD/iso_root/boot/limine/"
cp "$LIM/BOOTX64.EFI" "$BUILD/iso_root/EFI/BOOT/"
xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
  --efi-boot boot/limine/limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label \
  "$BUILD/iso_root" -o "$BUILD/$NAME" >/dev/null 2>&1
"$LIM/limine" bios-install "$BUILD/$NAME" >/dev/null 2>&1
echo "$BUILD/$NAME"
