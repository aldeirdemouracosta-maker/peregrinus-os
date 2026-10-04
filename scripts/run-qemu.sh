#!/usr/bin/env sh
# Interactive QEMU run of one profile (serial on stdio). For automated checks use
# scripts/qemu-qualify.sh. Usage: run-qemu.sh [safe|dma-test|e1000] [bios|uefi]
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
PROFILE=${1:-safe}
FIRMWARE=${2:-uefi}
command -v qemu-system-x86_64 >/dev/null 2>&1 || { echo "ERROR: qemu-system-x86_64 is required" >&2; exit 1; }
FW=""
if [ "$FIRMWARE" = uefi ]; then
  for f in /usr/share/ovmf/OVMF.fd /usr/share/OVMF/OVMF.fd /usr/share/qemu/OVMF.fd; do [ -f "$f" ] && { FW="-bios $f"; break; }; done
  if [ -z "$FW" ]; then
    for f in /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/OVMF/OVMF_CODE.fd /usr/share/edk2/x64/OVMF_CODE.fd /usr/share/edk2-ovmf/x64/OVMF_CODE.fd; do
      [ -f "$f" ] && { FW="-drive if=pflash,format=raw,readonly=on,file=$f"; break; }
    done
  fi
  [ -n "$FW" ] || { echo "ERROR: OVMF not found (install the ovmf package) or pass 'bios'" >&2; exit 1; }
fi
DISPLAY_ARGS="-display none"; [ -n "${DISPLAY:-}" ] && DISPLAY_ARGS="-display gtk"
ISO=$(./scripts/make-iso.sh "$PROFILE" | tail -1)
case "$PROFILE" in
 safe) EXTRA="" ;;
 dma-test)
   DISK=build/peregrinus-testdisk.img
   [ -f "$DISK" ] || ./scripts/create-gpt-test-image.py "$DISK"
   EXTRA="-boot d -drive if=none,id=peregrinus_test,format=raw,file=$DISK,snapshot=on -device ide-hd,drive=peregrinus_test,bus=ide.0" ;;
 e1000) EXTRA="-netdev user,id=net0 -device e1000,netdev=net0" ;;
 *) echo "usage: $0 [safe|dma-test|e1000] [bios|uefi]" >&2; exit 2 ;;
esac
# shellcheck disable=SC2086
exec qemu-system-x86_64 -machine q35 -m 512M -cpu max -serial stdio $DISPLAY_ARGS $FW -cdrom "$ISO" $EXTRA -no-reboot -no-shutdown
