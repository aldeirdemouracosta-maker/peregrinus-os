#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
PROFILE=${1:-safe}
command -v xorriso >/dev/null 2>&1 || { echo "ERROR: xorriso is required" >&2; exit 1; }
LIMINE=third_party/limine-bin
[ -x "$LIMINE/limine" ] || { echo "ERROR: Limine not built. Run scripts/fetch-limine.sh" >&2; exit 1; }
for f in limine-bios.sys limine-bios-cd.bin limine-uefi-cd.bin; do
  [ -f "$LIMINE/$f" ] || { echo "ERROR: missing $LIMINE/$f" >&2; exit 1; }
done
case "$PROFILE" in
  safe)     make;                             BUILD=build;                       TITLE="SAFE" ;;
  dma-test) make dma-test-kernel;             BUILD=build-qemu-dma;              TITLE="QEMU QUALIFICATION: read-only disposable disk" ;;
  e1000)    make qemu-e1000-sandbox;          BUILD=build-qemu-e1000;            TITLE="QEMU QUALIFICATION: LIVE e1000 NIC" ;;
  journal)  make recovery-journal-test-kernel; BUILD=build-recovery-journal-test; TITLE="QEMU QUALIFICATION: WRITES disk (journal test)" ;;
  commit)   make recovery-commit-test-kernel;  BUILD=build-recovery-commit-test;  TITLE="QEMU QUALIFICATION: WRITES disk (boot-success commit)" ;;
  *) echo "usage: $0 [safe|dma-test|e1000|journal|commit]" >&2; exit 2 ;;
esac
NAME=peregrinus-$PROFILE.iso
RELEASE=$(./scripts/release-profile.py name)
rm -rf "$BUILD/iso_root"
mkdir -p "$BUILD/iso_root/boot/limine"
cp "$BUILD/peregrinus.elf" "$BUILD/iso_root/boot/peregrinus.elf"
# The boot menu names the real profile, so a live/write qualification image
# can never be mistaken for the SAFE build.
sed "s|^/.*|/Peregrinus OS — $RELEASE — $TITLE|" limine.conf > "$BUILD/iso_root/boot/limine/limine.conf"
cp "$LIMINE/limine-bios.sys" "$LIMINE/limine-bios-cd.bin" "$LIMINE/limine-uefi-cd.bin" "$BUILD/iso_root/boot/limine/"
xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
  --efi-boot boot/limine/limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label \
  "$BUILD/iso_root" -o "$BUILD/$NAME"
"$LIMINE/limine" bios-install "$BUILD/$NAME"
sha256sum "$BUILD/$NAME"
