#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
PROFILE=${1:-safe}
command -v qemu-system-x86_64 >/dev/null 2>&1 || { echo "ERROR: qemu-system-x86_64 is required" >&2; exit 1; }
OVMF=""
for f in /usr/share/OVMF/OVMF_CODE.fd /usr/share/edk2/x64/OVMF_CODE.fd /usr/share/edk2-ovmf/x64/OVMF_CODE.fd; do [ -f "$f" ] && { OVMF="$f"; break; }; done
FW=""; [ -n "$OVMF" ] && FW="-bios $OVMF"
case "$PROFILE" in
 safe)
   ISO=${2:-build/peregrinus-muro-1.0-safe.iso}
   [ -f "$ISO" ] || { echo "ERROR: ISO not found: $ISO" >&2; exit 1; }
   exec qemu-system-x86_64 -machine q35 -m 512M -smp 2 -cpu max -serial stdio -display gtk $FW -cdrom "$ISO" -no-reboot -no-shutdown
   ;;
 dma-test)
   ISO=${2:-build-qemu-dma/peregrinus-muro-1.0-qemu-dma.iso}
   DISK=${3:-build/peregrinus-muro-1.0-testdisk.img}
   [ -f "$ISO" ] || { echo "ERROR: ISO not found: $ISO" >&2; exit 1; }
   [ -f "$DISK" ] || { echo "ERROR: test disk not found: $DISK" >&2; exit 1; }
   exec qemu-system-x86_64 -machine q35 -m 512M -smp 2 -cpu max -serial stdio -nographic $FW \
     -cdrom "$ISO" -drive if=none,id=peregrinus_test,format=raw,file="$DISK",snapshot=on \
     -device ide-hd,drive=peregrinus_test,bus=ide.0 -no-reboot -no-shutdown
   ;;
 e1000)
   ISO=${2:-build-qemu-e1000/peregrinus-muro-1.0-e1000-sandbox.iso}
   [ -f "$ISO" ] || { echo "ERROR: ISO not found: $ISO" >&2; exit 1; }
   exec qemu-system-x86_64 -machine q35 -m 512M -smp 2 -cpu max -serial stdio -nographic $FW \
     -cdrom "$ISO" -netdev user,id=net0 -device e1000,netdev=net0 -no-reboot -no-shutdown
   ;;
 *) echo "usage: $0 [safe|dma-test|e1000] [iso] [disk]" >&2; exit 2 ;;
esac
