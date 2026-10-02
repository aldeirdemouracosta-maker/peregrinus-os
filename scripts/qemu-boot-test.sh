#!/usr/bin/env bash
# Boots Peregrinus profiles in QEMU (UEFI/OVMF, headless), captures the serial
# log and asserts the expected boot markers. Unless a step explicitly expects
# one, any panic, CPU exception or missing marker fails.
#
#   safe     SAFE kernel, no disk: must halt fail-closed (no verified boot health)
#   disk     read-only disposable GPT disk: must reach BOOT-READONLY and finish
#   e1000    e1000 qualification + read-only disk: datapath READY, then a network
#            peer checks ARP reply, ICMP echo reply and UDP default-deny
#   journal  recovery-journal write test on a writable disk copy, booted twice:
#            attempt+success transaction PASS each time, no split-brain
#   commit   direct boot-success commit: CONFIRMED only after a pending attempt
#            recorded by pre-boot; rejected (fail-closed panic) without one
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
PROFILE=${1:?usage: $0 safe|disk|e1000|journal|commit}
TIMEOUT=${QEMU_BOOT_TIMEOUT:-120}
OUT=${QEMU_BOOT_OUT:-build-boot-test}
BASE_DISK=build/peregrinus-muro-1.0.1-testdisk.img
mkdir -p "$OUT"

OVMF_CODE=""; OVMF_VARS=""
for d in /usr/share/OVMF /usr/share/edk2/x64 /usr/share/edk2-ovmf/x64; do
  for suffix in _4M ""; do
    if [ -f "$d/OVMF_CODE$suffix.fd" ] && [ -f "$d/OVMF_VARS$suffix.fd" ]; then
      OVMF_CODE="$d/OVMF_CODE$suffix.fd"; OVMF_VARS="$d/OVMF_VARS$suffix.fd"; break 2
    fi
  done
done
[ -n "$OVMF_CODE" ] || { echo "ERROR: OVMF firmware not found" >&2; exit 1; }

ACCEL=(-accel tcg)
[ -w /dev/kvm ] && ACCEL=(-accel kvm -accel tcg)

FAIL=0
STD_FORBID=('PEREGRINUS KERNEL PANIC' 'CPU EXCEPTION')

# boot NAME ISO DISK_MODE DISK_PATH [NET_PORT]
#   DISK_MODE: none | ro (snapshot, never written) | rw (writes persist to DISK_PATH)
# Uses the arrays EXPECT and FORBID; the last EXPECT entry ends the boot early.
boot() {
  local name=$1 iso=$2 mode=$3 disk=${4:-} port=${5:-}
  local log="$OUT/$PROFILE-$name-serial.log" clean="$OUT/$PROFILE-$name-serial.txt"
  rm -f "$log"
  cp "$OVMF_VARS" "$OUT/$PROFILE-$name-vars.fd"
  local q=(qemu-system-x86_64 -machine q35 -m 512M -smp 2 -cpu max "${ACCEL[@]}"
    -display none -monitor none -serial "file:$log" -no-reboot
    -drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE"
    -drive "if=pflash,format=raw,file=$OUT/$PROFILE-$name-vars.fd" -cdrom "$iso")
  case "$mode" in
    ro) q+=(-drive "if=none,id=testdisk,format=raw,file=$disk,snapshot=on" -device ide-hd,drive=testdisk,bus=ide.0) ;;
    rw) q+=(-drive "if=none,id=testdisk,format=raw,file=$disk" -device ide-hd,drive=testdisk,bus=ide.0) ;;
  esac
  [ -n "$port" ] && q+=(-netdev "socket,id=net0,listen=127.0.0.1:$port" -device e1000,netdev=net0)

  echo "== QEMU boot: $PROFILE/$name (timeout ${TIMEOUT}s)"
  timeout "$TIMEOUT" "${q[@]}" &
  local qpid=$! peer_rc=0
  if [ -n "$port" ]; then
    python3 tests/qemu_net_peer.py "$port" "$((TIMEOUT - 10))" || peer_rc=$?
  else
    # The kernel ends in hlt or a panic loop; stop once the final marker shows.
    local last=${EXPECT[${#EXPECT[@]}-1]}
    for _ in $(seq 1 "$TIMEOUT"); do
      grep -qF -- "$last" "$log" 2>/dev/null && break
      kill -0 "$qpid" 2>/dev/null || break
      sleep 1
    done
    sleep 1
  fi
  kill "$qpid" 2>/dev/null || true
  wait "$qpid" 2>/dev/null || true

  tr -d '\r' < "$log" | sed 's/\x1b\[[0-9;=]*[A-Za-z]//g' > "$clean"
  local bad=0 m
  for m in "${EXPECT[@]}"; do
    if grep -qF -- "$m" "$clean"; then echo "  ok: $m"; else echo "  MISSING: $m"; bad=1; fi
  done
  for m in "${FORBID[@]}"; do
    if grep -qF -- "$m" "$clean"; then echo "  FORBIDDEN: $m"; bad=1; fi
  done
  [ "$peer_rc" -eq 0 ] || { echo "  network peer failed (rc=$peer_rc)"; bad=1; }
  if [ "$bad" -ne 0 ]; then
    echo "---- serial log ($clean) ----"; cat "$clean"; FAIL=1
  fi
}

ensure_base_disk() { [ -f "$BASE_DISK" ] || make qemu-test-disk >/dev/null; }
disk_sha() { sha256sum "$1" | cut -d' ' -f1; }

case "$PROFILE" in
  safe)
    ./scripts/make-iso.sh safe >"$OUT/$PROFILE-iso.log" 2>&1 || { cat "$OUT/$PROFILE-iso.log"; exit 1; }
    EXPECT=('Kernel .text SHA-256: PASS' 'Firewall NIC datapath hook: BLOCKED'
            'NIC probe: read-only; BAR/MMIO/DMA/interrupt setup BLOCKED'
            'Boot health: UNAVAILABLE' 'PEREGRINUS GUARD: fail-closed halt.')
    FORBID=("${STD_FORBID[@]}")
    boot nodisk build/peregrinus-muro-1.0-safe.iso none
    ;;
  disk)
    ./scripts/make-iso.sh dma-test >"$OUT/$PROFILE-iso.log" 2>&1 || { cat "$OUT/$PROFILE-iso.log"; exit 1; }
    ensure_base_disk
    before=$(disk_sha "$BASE_DISK")
    EXPECT=('Kernel .text SHA-256: PASS' 'GPT REDUNDANCY: HEALTHY; primary and backup agree'
            'Recovery anchor A: VALID' 'Recovery anchor B: VALID' 'Boot health: HEALTHY'
            'Peregrinus Guard action: BOOT-READONLY'
            'live NIC ownership remains BLOCKED')
    FORBID=("${STD_FORBID[@]}")
    boot readonly build-qemu-dma/peregrinus-muro-1.0-qemu-dma.iso ro "$BASE_DISK"
    [ "$(disk_sha "$BASE_DISK")" = "$before" ] || { echo "  read-only profile modified the disk image"; FAIL=1; }
    ;;
  e1000)
    ./scripts/make-iso.sh e1000 >"$OUT/$PROFILE-iso.log" 2>&1 || { cat "$OUT/$PROFILE-iso.log"; exit 1; }
    ensure_base_disk
    EXPECT=('Kernel .text SHA-256: PASS' 'Peregrinus Guard action: BOOT-READONLY'
            'e1000 sandbox: RX/TX polling rings READY; interrupts disabled'
            'Muro e1000 sandbox init: READY')
    FORBID=("${STD_FORBID[@]}")
    boot net build-qemu-e1000/peregrinus-muro-1.0-e1000-sandbox.iso ro "$BASE_DISK" \
      "${QEMU_NET_PORT:-$((20000 + RANDOM % 20000))}"
    ;;
  journal)
    ./scripts/make-iso.sh journal >"$OUT/$PROFILE-iso.log" 2>&1 || { cat "$OUT/$PROFILE-iso.log"; exit 1; }
    ensure_base_disk
    DISK="$OUT/journal-disk.img"; cp "$BASE_DISK" "$DISK"
    EXPECT=('Kernel .text SHA-256: PASS' 'Recovery journal attempt+success transaction: PASS'
            'Peregrinus Guard action: BOOT-READONLY'
            'live NIC ownership remains BLOCKED')
    FORBID=("${STD_FORBID[@]}" 'SPLIT-BRAIN')
    for n in 1 2; do
      before=$(disk_sha "$DISK")
      boot "boot$n" build-recovery-journal-test/peregrinus-recovery-journal-test.iso rw "$DISK"
      [ "$(disk_sha "$DISK")" != "$before" ] || { echo "  boot$n did not persist the journal transaction"; FAIL=1; }
    done
    ;;
  commit)
    ./scripts/make-iso.sh commit >"$OUT/$PROFILE-iso.log" 2>&1 || { cat "$OUT/$PROFILE-iso.log"; exit 1; }
    ISO=build-recovery-commit-test/peregrinus-recovery-commit-test.iso
    DISK="$OUT/commit-disk.img"
    ./scripts/create-gpt-test-image.py --pending-current-attempt "$DISK" >/dev/null
    # 1. Pending attempt recorded by pre-boot: the kernel confirms success.
    EXPECT=('Kernel .text SHA-256: PASS' 'IA_RECOVERY direct boot-success commit: CONFIRMED'
            'live NIC ownership remains BLOCKED')
    FORBID=("${STD_FORBID[@]}" 'SPLIT-BRAIN')
    boot pending "$ISO" rw "$DISK"
    # 2. Same disk, no new attempt: a second success must be refused, fail-closed.
    EXPECT=('IA_RECOVERY direct boot-success commit: MARK-REJECTED'
            'Reason: Peregrinus direct recovery-journal success commit failed')
    FORBID=('IA_RECOVERY direct boot-success commit: CONFIRMED' 'CPU EXCEPTION' 'live NIC ownership')
    boot replay "$ISO" rw "$DISK"
    # 3. Baseline image never had an attempt: no success can be manufactured.
    ensure_base_disk
    boot no-attempt "$ISO" ro "$BASE_DISK"
    ;;
  *) echo "usage: $0 safe|disk|e1000|journal|commit" >&2; exit 2 ;;
esac

if [ "$FAIL" -ne 0 ]; then echo "FAIL: QEMU boot test $PROFILE"; exit 1; fi
echo "PASS: QEMU boot test $PROFILE"
