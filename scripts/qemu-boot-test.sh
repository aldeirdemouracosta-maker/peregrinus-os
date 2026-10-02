#!/usr/bin/env bash
# Boots a Peregrinus profile in QEMU (UEFI/OVMF, headless), captures the serial
# log and asserts the expected boot markers. Any panic or missing marker fails.
#
#   safe   SAFE kernel, no disk: must halt fail-closed (no verified boot health)
#   disk   read-only disposable GPT disk: must reach BOOT-READONLY and finish
#   e1000  e1000 qualification + read-only disk: datapath READY, then a network
#          peer checks ARP reply, ICMP echo reply and UDP default-deny
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
PROFILE=${1:?usage: $0 safe|disk|e1000}
TIMEOUT=${QEMU_BOOT_TIMEOUT:-120}
OUT=${QEMU_BOOT_OUT:-build-boot-test}
mkdir -p "$OUT"
LOG="$OUT/$PROFILE-serial.log"
rm -f "$LOG"

OVMF_CODE=""; OVMF_VARS=""
for d in /usr/share/OVMF /usr/share/edk2/x64 /usr/share/edk2-ovmf/x64; do
  for suffix in _4M ""; do
    if [ -f "$d/OVMF_CODE$suffix.fd" ] && [ -f "$d/OVMF_VARS$suffix.fd" ]; then
      OVMF_CODE="$d/OVMF_CODE$suffix.fd"; OVMF_VARS="$d/OVMF_VARS$suffix.fd"; break 2
    fi
  done
done
[ -n "$OVMF_CODE" ] || { echo "ERROR: OVMF firmware not found" >&2; exit 1; }
cp "$OVMF_VARS" "$OUT/$PROFILE-vars.fd"

ACCEL=(-accel tcg)
[ -w /dev/kvm ] && ACCEL=(-accel kvm -accel tcg)

QEMU=(qemu-system-x86_64 -machine q35 -m 512M -smp 2 -cpu max "${ACCEL[@]}"
  -display none -monitor none -serial "file:$LOG" -no-reboot
  -drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE"
  -drive "if=pflash,format=raw,file=$OUT/$PROFILE-vars.fd")

ensure_disk() {
  DISK=build/peregrinus-muro-1.0.1-testdisk.img
  [ -f "$DISK" ] || make qemu-test-disk >/dev/null
  QEMU+=(-drive "if=none,id=testdisk,format=raw,file=$DISK,snapshot=on" -device ide-hd,drive=testdisk,bus=ide.0)
}

EXPECT=()
FORBID=('PEREGRINUS KERNEL PANIC' 'CPU EXCEPTION')
PEER_RC=0
case "$PROFILE" in
  safe)
    ./scripts/make-iso.sh safe >/dev/null
    QEMU+=(-cdrom build/peregrinus-muro-1.0-safe.iso)
    EXPECT=('Kernel .text SHA-256: PASS' 'Firewall NIC datapath hook: BLOCKED'
            'NIC probe: read-only; BAR/MMIO/DMA/interrupt setup BLOCKED'
            'Boot health: UNAVAILABLE' 'PEREGRINUS GUARD: fail-closed halt.')
    ;;
  disk)
    ./scripts/make-iso.sh dma-test >/dev/null
    ensure_disk
    QEMU+=(-cdrom build-qemu-dma/peregrinus-muro-1.0-qemu-dma.iso)
    EXPECT=('Kernel .text SHA-256: PASS' 'GPT REDUNDANCY: HEALTHY; primary and backup agree'
            'Recovery anchor A: VALID' 'Recovery anchor B: VALID' 'Boot health: HEALTHY'
            'Peregrinus Guard action: BOOT-READONLY'
            'live NIC ownership remains BLOCKED')
    ;;
  e1000)
    ./scripts/make-iso.sh e1000 >/dev/null
    ensure_disk
    PORT=${QEMU_NET_PORT:-$((20000 + RANDOM % 20000))}
    QEMU+=(-cdrom build-qemu-e1000/peregrinus-muro-1.0-e1000-sandbox.iso
           -netdev "socket,id=net0,listen=127.0.0.1:$PORT" -device e1000,netdev=net0)
    EXPECT=('Kernel .text SHA-256: PASS' 'Peregrinus Guard action: BOOT-READONLY'
            'e1000 sandbox: RX/TX polling rings READY; interrupts disabled'
            'Muro e1000 sandbox init: READY')
    ;;
  *) echo "usage: $0 safe|disk|e1000" >&2; exit 2 ;;
esac

echo "== QEMU boot test: $PROFILE (timeout ${TIMEOUT}s)"
timeout "$TIMEOUT" "${QEMU[@]}" &
QPID=$!
if [ "$PROFILE" = e1000 ]; then
  python3 tests/qemu_net_peer.py "$PORT" "$((TIMEOUT - 10))" || PEER_RC=$?
  kill "$QPID" 2>/dev/null || true
else
  # The kernel ends in hlt; stop as soon as the final marker appears.
  last=${EXPECT[${#EXPECT[@]}-1]}
  for _ in $(seq 1 "$TIMEOUT"); do
    grep -qF -- "$last" "$LOG" 2>/dev/null && break
    kill -0 "$QPID" 2>/dev/null || break
    sleep 1
  done
  kill "$QPID" 2>/dev/null || true
fi
wait "$QPID" 2>/dev/null || true

CLEAN="$OUT/$PROFILE-serial.txt"
tr -d '\r' < "$LOG" | sed 's/\x1b\[[0-9;=]*[A-Za-z]//g' > "$CLEAN"
FAIL=0
for m in "${EXPECT[@]}"; do
  if grep -qF -- "$m" "$CLEAN"; then echo "  ok: $m"; else echo "  MISSING: $m"; FAIL=1; fi
done
for m in "${FORBID[@]}"; do
  if grep -qF -- "$m" "$CLEAN"; then echo "  FORBIDDEN: $m"; FAIL=1; fi
done
[ "$PEER_RC" -eq 0 ] || { echo "  network peer failed (rc=$PEER_RC)"; FAIL=1; }
if [ "$FAIL" -ne 0 ]; then
  echo "---- serial log ($CLEAN) ----"; cat "$CLEAN"
  echo "FAIL: QEMU boot test $PROFILE"; exit 1
fi
echo "PASS: QEMU boot test $PROFILE"
