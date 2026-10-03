#!/usr/bin/env sh
# Interactive/manual QEMU run (serial on stdio). For automated qualification
# use scripts/qemu-boot-test.sh, which asserts the boot markers.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
PROFILE=${1:-safe}
command -v qemu-system-x86_64 >/dev/null 2>&1 || { echo "ERROR: qemu-system-x86_64 is required" >&2; exit 1; }

# UEFI firmware as split CODE/VARS pflash (Ubuntu ships the *_4M variants).
OVMF_CODE=""; OVMF_VARS=""
for d in /usr/share/OVMF /usr/share/edk2/x64 /usr/share/edk2-ovmf/x64; do
  for suffix in _4M ""; do
    if [ -z "$OVMF_CODE" ] && [ -f "$d/OVMF_CODE$suffix.fd" ] && [ -f "$d/OVMF_VARS$suffix.fd" ]; then
      OVMF_CODE="$d/OVMF_CODE$suffix.fd"; OVMF_VARS="$d/OVMF_VARS$suffix.fd"
    fi
  done
done
mkdir -p build
if [ -n "$OVMF_CODE" ]; then
  cp "$OVMF_VARS" build/run-qemu-vars.fd
  set -- -drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE" -drive "if=pflash,format=raw,file=build/run-qemu-vars.fd"
else
  echo "WARNING: OVMF not found; booting with legacy BIOS" >&2
  set --
fi

DISK=build/peregrinus-testdisk.img
need() { [ -f "$1" ] || { echo "ERROR: not found: $1 ($2)" >&2; exit 1; }; }
case "$PROFILE" in
  safe)
    need build/peregrinus-safe.iso "run scripts/make-iso.sh safe"
    set -- "$@" -cdrom build/peregrinus-safe.iso ;;
  dma-test)
    need build-qemu-dma/peregrinus-dma-test.iso "run scripts/make-iso.sh dma-test"
    need "$DISK" "run make qemu-test-disk"
    set -- "$@" -cdrom build-qemu-dma/peregrinus-dma-test.iso \
      -drive "if=none,id=peregrinus_test,format=raw,file=$DISK,snapshot=on" -device ide-hd,drive=peregrinus_test,bus=ide.0 ;;
  e1000)
    need build-qemu-e1000/peregrinus-e1000.iso "run scripts/make-iso.sh e1000"
    need "$DISK" "run make qemu-test-disk"
    set -- "$@" -cdrom build-qemu-e1000/peregrinus-e1000.iso \
      -drive "if=none,id=peregrinus_test,format=raw,file=$DISK,snapshot=on" -device ide-hd,drive=peregrinus_test,bus=ide.0 \
      -netdev user,id=net0 -device e1000,netdev=net0 ;;
  *) echo "usage: $0 [safe|dma-test|e1000]" >&2; exit 2 ;;
esac
exec qemu-system-x86_64 -machine q35 -m 512M -smp 2 -cpu max -nographic -serial mon:stdio -no-reboot -no-shutdown "$@"
