#!/usr/bin/env bash
# Runtime qualification: boot real kernels under QEMU and check what they print on the
# serial port. This is the test layer that host unit tests cannot replace — it exercises
# the whole kmain() path of each profile.
#
# Needs: qemu-system-x86_64, OVMF, xorriso, python3 and a Limine build
# (scripts/fetch-limine.sh). Usage: scripts/qemu-qualify.sh [case...]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
OUT=build-qualify
mkdir -p "$OUT"
QEMU=(qemu-system-x86_64 -machine q35 -m 256M -cpu max -display none -no-reboot -no-shutdown)
# UEFI firmware: a combined image works with -bios; split CODE images need a read-only pflash.
OVMF=(); 
for f in /usr/share/ovmf/OVMF.fd /usr/share/OVMF/OVMF.fd /usr/share/qemu/OVMF.fd; do [[ -f "$f" ]] && { OVMF=(-bios "$f"); break; }; done
if [[ ${#OVMF[@]} -eq 0 ]]; then
  for f in /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/OVMF/OVMF_CODE.fd /usr/share/edk2/x64/OVMF_CODE.fd /usr/share/edk2-ovmf/x64/OVMF_CODE.fd; do
    [[ -f "$f" ]] && { OVMF=(-drive if=pflash,format=raw,readonly=on,file="$f"); break; }
  done
fi
FAILS=0

# boot <log> <done-regex> <seconds> <qemu args...>: run until the regex appears or time runs out.
boot(){
  local log="$OUT/$1" done_re="$2" secs="$3"; shift 3
  rm -f "$log"
  "${QEMU[@]}" -serial "file:$log" "$@" & local pid=$!
  local t=0
  while (( t < secs*5 )); do
    if [[ -f "$log" ]] && grep -aqE "$done_re" "$log"; then break; fi
    if ! kill -0 "$pid" 2>/dev/null; then break; fi
    sleep 0.2; t=$((t+1))
  done
  kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true
}
expect(){ if grep -aqE "$2" "$OUT/$1"; then echo "  ok: $2"; else echo "  FAIL [$1]: missing /$2/"; FAILS=$((FAILS+1)); fi; }
forbid(){ if grep -aqE "$2" "$OUT/$1"; then echo "  FAIL [$1]: unexpected /$2/"; FAILS=$((FAILS+1)); else echo "  ok: no /$2/"; fi; }
FATAL='KERNEL PANIC|CPU EXCEPTION|fail-closed halt'

case_safe_bios(){
  echo "== SAFE profile, legacy BIOS"
  local iso; iso=$(./scripts/make-iso.sh safe | tail -1)
  boot safe-bios.log 'Shell: |PANIC|EXCEPTION|halt\.' 60 -cdrom "$iso"
  expect safe-bios.log 'Kernel \.text SHA-256'
  expect safe-bios.log 'Stack protector: ACTIVE'
  expect safe-bios.log 'Console: framebuffer text'
  expect safe-bios.log 'ACPI MADT LAPIC'
  expect safe-bios.log 'PEREGRINUS GUARD: passive boot'
  expect safe-bios.log 'Interrupts: PIC/PIT live'
  expect safe-bios.log 'live NIC ownership remains BLOCKED'
  forbid safe-bios.log "$FATAL"
}
case_safe_uefi(){
  echo "== SAFE profile, UEFI (OVMF)"
  [[ ${#OVMF[@]} -gt 0 ]] || { echo "  FAIL: OVMF not found"; FAILS=$((FAILS+1)); return; }
  local iso; iso=$(./scripts/make-iso.sh safe | tail -1)
  boot safe-uefi.log 'Shell: |PANIC|EXCEPTION|halt\.' 120 "${OVMF[@]}" -cdrom "$iso"
  expect safe-uefi.log 'ACPI MCFG ECAM base'
  expect safe-uefi.log 'PEREGRINUS GUARD: passive boot'
  expect safe-uefi.log 'Interrupts: PIC/PIT live'
  expect safe-uefi.log 'live NIC ownership remains BLOCKED'
  forbid safe-uefi.log "$FATAL"
}
case_e1000(){
  echo "== e1000 qualification profile: live datapath"
  local iso; iso=$(./scripts/make-iso.sh e1000 | tail -1)
  local log="$OUT/e1000.log"; rm -f "$log"
  "${QEMU[@]}" -serial "file:$log" -cdrom "$iso" \
    -netdev socket,id=n0,udp=127.0.0.1:5556,localaddr=127.0.0.1:5555 -device e1000,netdev=n0 & local pid=$!
  if python3 tests/qemu/net_inject.py 5556 5555 60; then echo "  ok: network expectations"; else FAILS=$((FAILS+1)); fi
  kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true
  expect e1000.log 'e1000 sandbox init: READY'
  forbid e1000.log "$FATAL"
}
# recovery_case <log> <make-iso profile>: boot a recovery-commit kernel on a disposable GPT disk
# whose journal carries the pending CURRENT attempt the pre-boot controller would have written.
recovery_case(){
  local name="$1" profile="$2"
  echo "== $profile: IA_RECOVERY boot-success commit on a disposable disk"
  local iso; iso=$(./scripts/make-iso.sh "$profile" | tail -1)
  local disk="$OUT/$name.img"
  ./scripts/create-gpt-test-image.py --pending-current "$disk" >/dev/null
  boot "$name.log" 'boot-success commit: [A-Z-]+|PANIC|EXCEPTION|halt\.' 90 -boot d -cdrom "$iso" \
    -drive if=none,id=d0,format=raw,file="$disk" -device ide-hd,drive=d0,bus=ide.0
  expect "$name.log" 'GPT selection: PRIMARY'
  expect "$name.log" 'IA_RECOVERY direct boot-success commit: CONFIRMED'
  forbid "$name.log" "$FATAL"
  if python3 tests/qemu/check_journal.py "$disk"; then echo "  ok: journal on disk records the success"; else FAILS=$((FAILS+1)); fi
}
case_recovery_commit_test(){ recovery_case recovery-commit-test recovery-commit-test; }
case_recovery_live(){ recovery_case recovery-live recovery-live; }
case_uefi_controller(){
  echo "== Trusted Boot Controller (UEFI, stack protector on) chainloads Limine and the SAFE kernel"
  [[ ${#OVMF[@]} -gt 0 ]] || { echo "  FAIL: OVMF not found"; FAILS=$((FAILS+1)); return; }
  ./scripts/build-trusted-boot-controller.sh "$OUT/bootctl" >/dev/null
  make -s >/dev/null
  local esp="$OUT/esp"; rm -rf "$esp"
  mkdir -p "$esp/EFI/BOOT" "$esp/EFI/Peregrinus/committed" "$esp/boot/limine"
  cp "$OUT/bootctl/peregrinus-boot-controller.efi" "$esp/EFI/BOOT/BOOTX64.EFI"
  cp third_party/limine-dist/bin/BOOTX64.EFI "$esp/EFI/Peregrinus/committed/limine_x64.efi"
  cp limine.conf "$esp/boot/limine/limine.conf"; cp build/peregrinus.elf "$esp/boot/peregrinus.elf"
  boot uefi-controller.log 'live NIC ownership remains BLOCKED|PANIC|EXCEPTION|halt\.' 120 "${OVMF[@]}" -drive format=raw,file=fat:rw:"$esp"
  expect uefi-controller.log 'PEREGRINUS GUARD: passive boot'
  forbid uefi-controller.log "$FATAL"
}
case_screen(){
  echo "== boot log is readable on the screen (framebuffer text console), not only on serial"
  local iso; iso=$(./scripts/make-iso.sh safe | tail -1)
  local mon="$OUT/screen.mon" log="$OUT/screen.log"; rm -f "$mon" "$log" "$OUT/screen.ppm"
  "${QEMU[@]}" -serial "file:$log" -cdrom "$iso" -monitor unix:"$mon",server,nowait & local pid=$!
  local t=0; while (( t < 300 )) && ! grep -aq 'Boot complete' "$log" 2>/dev/null; do sleep 0.2; t=$((t+1)); done
  sleep 1
  python3 - "$mon" "$PWD/$OUT/screen.ppm" <<'PY'
import socket, sys, time
s = socket.socket(socket.AF_UNIX); s.connect(sys.argv[1]); time.sleep(0.3); s.recv(4096)
s.sendall(('screendump %s\n' % sys.argv[2]).encode()); time.sleep(1.5); s.close()
PY
  kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true
  if python3 tests/qemu/screen_text.py "$OUT/screen.ppm" 'Hardware Manifest' 'PEREGRINUS GUARD: passive boot' 'Boot complete' > "$OUT/screen.txt"; then
    echo "  ok: boot log decoded from the screen pixels ($OUT/screen.txt)"
  else tail -3 "$OUT/screen.txt"; FAILS=$((FAILS+1)); fi
}
case_shell(){
  echo "== shell: commands over serial and over the emulated PS/2 keyboard; prompt and accents on screen"
  local iso; iso=$(./scripts/make-iso.sh safe | tail -1)
  local ser="$OUT/shell.ser" mon="$OUT/shell.mon"; rm -f "$ser" "$mon" "$OUT/shell.ppm"
  "${QEMU[@]}" -chardev socket,id=s0,path="$ser",server=on,wait=off -serial chardev:s0 \
    -monitor unix:"$mon",server,nowait -cdrom "$iso" & local pid=$!
  if python3 tests/qemu/shell_drive.py "$ser" "$mon" "$PWD/$OUT/shell.ppm" > "$OUT/shell.txt" 2>&1; then
    sed 's/^/  /' "$OUT/shell.txt"
  else tail -5 "$OUT/shell.txt"; FAILS=$((FAILS+1)); fi
  kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true
  if python3 tests/qemu/screen_text.py "$OUT/shell.ppm" 'peregrinus> sobre' 'Geração' 'RAM utilizável' 'Comando desconhecido: çáõ' > "$OUT/shell-screen.txt"; then
    echo "  ok: prompt, typed commands and accented output decoded from the screen"
  else tail -3 "$OUT/shell-screen.txt"; FAILS=$((FAILS+1)); fi
}
case_no_serial(){
  echo "== machine without a serial port: boot and shell still reach the screen, with no per-character delay"
  local iso; iso=$(./scripts/make-iso.sh safe | tail -1)
  local mon="$OUT/noserial.mon"; rm -f "$mon" "$OUT/noserial.ppm"
  "${QEMU[@]}" -serial none -monitor unix:"$mon",server,nowait -cdrom "$iso" & local pid=$!
  sleep 20
  python3 - "$mon" "$PWD/$OUT/noserial.ppm" <<'PY'
import socket, sys, time
s = socket.socket(socket.AF_UNIX)
for _ in range(50):
    try: s.connect(sys.argv[1]); break
    except OSError: time.sleep(0.2)
time.sleep(0.3); s.recv(4096)
for k in ['h', 'w', 'ret']:
    s.sendall(('sendkey %s\n' % k).encode()); time.sleep(0.2)
time.sleep(1.0)
s.sendall(('screendump %s\n' % sys.argv[2]).encode()); time.sleep(1.5); s.close()
PY
  kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true
  if python3 tests/qemu/screen_text.py "$OUT/noserial.ppm" 'Boot complete' 'peregrinus> hw' 'RAM utilizável' > "$OUT/noserial.txt"; then
    echo "  ok: booted within 20 s and the keyboard shell answered, without COM1"
  else tail -3 "$OUT/noserial.txt"; FAILS=$((FAILS+1)); fi
}
case_idle(){
  echo "== interrupts: idle shell halts the CPU (hlt) and the PIT timer runs"
  local iso; iso=$(./scripts/make-iso.sh safe | tail -1)
  local ser="$OUT/idle.ser"; rm -f "$ser"
  "${QEMU[@]}" -chardev socket,id=s0,path="$ser",server=on,wait=off -serial chardev:s0 -cdrom "$iso" & local pid=$!
  if python3 tests/qemu/idle_check.py "$ser" "$pid" > "$OUT/idle.txt" 2>&1; then sed 's/^/  ok: /' "$OUT/idle.txt"
  else cat "$OUT/idle.txt"; FAILS=$((FAILS+1)); fi
  kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true
}
case_double_fault(){
  echo "== kernel stack overflow hits the guard page and is reported (no silent triple fault)"
  local iso; iso=$(./scripts/make-iso.sh double-fault-test | tail -1)
  boot double-fault.log 'Vector: [0-9]+|PANIC' 60 -cdrom "$iso"
  expect double-fault.log 'Vector: 8'
}

CASES=("$@")
[[ ${#CASES[@]} -gt 0 ]] || CASES=(safe_bios safe_uefi uefi_controller screen shell no_serial idle e1000 recovery_commit_test recovery_live double_fault)
for c in "${CASES[@]}"; do "case_$c"; done
if (( FAILS )); then echo "QEMU qualification: $FAILS failure(s); serial logs in $OUT/"; exit 1; fi
echo "QEMU qualification: PASS (${CASES[*]})"
