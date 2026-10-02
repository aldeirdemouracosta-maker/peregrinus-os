#!/usr/bin/env bash
# Full trusted-boot chain in QEMU: OVMF with Secure Boot (Ubuntu "snakeoil"
# test keys) -> signed Peregrinus boot controller -> TPM NV counter (swtpm)
# -> IA_RECOVERY pre-boot journal attempt -> signed Limine with enrolled config
# hash -> hash-pinned recovery-live kernel -> boot-success commit.
#
# The controller under test is the recovery-live qualification build
# (`make preboot-recovery-controller`, TPM commit base 41 / next 42). Each
# scenario provisions a fresh swtpm whose counter sits at the value under test.
# Test keys only: nothing produced here is a release artifact.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
TIMEOUT=${QEMU_BOOT_TIMEOUT:-150}
OUT=${QEMU_BOOT_OUT:-build-trusted-chain}
TPM_INDEX=0x0180F050
SB_KEY=/usr/share/ovmf/PkKek-1-snakeoil.key
SB_CERT=/usr/share/ovmf/PkKek-1-snakeoil.pem
OVMF_SB_CODE=/usr/share/OVMF/OVMF_CODE_4M.snakeoil.fd
OVMF_SB_VARS=/usr/share/OVMF/OVMF_VARS_4M.snakeoil.fd
OVMF_PLAIN_CODE=/usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_PLAIN_VARS=/usr/share/OVMF/OVMF_VARS_4M.fd
for f in "$SB_KEY" "$SB_CERT" "$OVMF_SB_CODE" "$OVMF_SB_VARS" "$OVMF_PLAIN_CODE" "$OVMF_PLAIN_VARS"; do
  [ -f "$f" ] || { echo "ERROR: missing $f (install ovmf)" >&2; exit 1; }
done
for t in swtpm tpm2_nvdefine tpm2_nvincrement tpm2_nvread sbsign mkfs.fat mcopy qemu-system-x86_64; do
  command -v "$t" >/dev/null || { echo "ERROR: missing $t (swtpm tpm2-tools sbsigntool dosfstools mtools qemu-system-x86)" >&2; exit 1; }
done
rm -rf "$OUT"; mkdir -p "$OUT"
# Ubuntu ships the public snakeoil test key encrypted with the passphrase
# "snakeoil"; decrypt it into the (git-ignored) test directory.
openssl pkey -in "$SB_KEY" -passin pass:snakeoil -out "$OUT/snakeoil.key"
SB_KEY="$OUT/snakeoil.key"

ACCEL=(-accel tcg)
[ -w /dev/kvm ] && ACCEL=(-accel kvm -accel tcg)

echo "== build"
{
  ./scripts/fetch-limine.sh
  make recovery-live-slots
  make preboot-recovery-controller
} > "$OUT/build.log" 2>&1 || { cat "$OUT/build.log"; exit 1; }
LIMINE=third_party/limine-bin
SLOTS=build-recovery-live-secure/boot

blake2b() { python3 -c 'import hashlib,sys;print(hashlib.blake2b(open(sys.argv[1],"rb").read()).hexdigest())' "$1"; }
sign() { sbsign --key "$SB_KEY" --cert "$SB_CERT" --output "$2" "$1" >/dev/null 2>&1; }

# ESP tree: controller as the removable-media boot path, one signed Limine per
# trust state with its config hash enrolled, hash-pinned kernels.
make_esp_tree() {  # make_esp_tree DIR SIGN_CONTROLLER(0|1)
  local d=$1 st
  mkdir -p "$d/EFI/BOOT" "$d/boot"
  if [ "$2" = 1 ]; then sign build-bootctl-preboot/peregrinus-boot-controller.efi "$d/EFI/BOOT/BOOTX64.EFI"
  else cp build-bootctl-preboot/peregrinus-boot-controller.efi "$d/EFI/BOOT/BOOTX64.EFI"; fi
  for st in staged committed; do
    mkdir -p "$d/EFI/Peregrinus/$st"
    cp "$SLOTS/limine/limine-$st.conf" "$d/EFI/Peregrinus/$st/limine.conf"
    cp "$LIMINE/BOOTX64.EFI" "$OUT/limine-$st.efi"
    "$LIMINE/limine" enroll-config "$OUT/limine-$st.efi" "$(blake2b "$d/EFI/Peregrinus/$st/limine.conf")" >/dev/null
    sign "$OUT/limine-$st.efi" "$d/EFI/Peregrinus/$st/limine_x64.efi"
  done
  cp "$SLOTS/peregrinus-current.elf" "$SLOTS/peregrinus-lkg.elf" "$d/boot/"
}

ESP_MIB=24; DISK_MIB=96
make_disk() {  # make_disk OUT_IMG ESP_TREE
  local img=$1 tree=$2 fat="$OUT/esp-$$.fat"
  ./scripts/create-gpt-test-image.py --mib "$DISK_MIB" --esp-mib "$ESP_MIB" "$img" >/dev/null
  rm -f "$fat"; mkfs.fat -C -F 16 -n PGR_ESP "$fat" $((ESP_MIB * 1024)) >/dev/null
  MTOOLS_SKIP_CHECK=1 mcopy -s -i "$fat" "$tree"/* ::/
  local total=$((DISK_MIB * 2048)) start
  start=$(( total - 34 - ESP_MIB * 2048 + 1 ))
  dd if="$fat" of="$img" bs=512 seek="$start" conv=notrunc status=none
  rm -f "$fat"
}

echo "== ESP + disk"
make_esp_tree "$OUT/esp-signed" 1
make_esp_tree "$OUT/esp-unsigned-controller" 0
make_disk "$OUT/base.img" "$OUT/esp-signed"
make_disk "$OUT/unsigned.img" "$OUT/esp-unsigned-controller"
# Tampered kernel: one flipped byte in .text of the CURRENT kernel on the ESP.
cp -r "$OUT/esp-signed" "$OUT/esp-tampered"
python3 - "$OUT/esp-tampered/boot/peregrinus-current.elf" <<'PY'
import sys
p=sys.argv[1]; b=bytearray(open(p,'rb').read()); b[0x1100]^=0xff; open(p,'wb').write(b)
PY
make_disk "$OUT/tampered.img" "$OUT/esp-tampered"

# provision_tpm STATE_DIR COUNTER  (fresh TPM, counter index defined and advanced)
provision_tpm() {
  local dir=$1 want=$2 port=$((20000 + RANDOM % 20000)) cur
  mkdir -p "$dir"
  swtpm socket --tpm2 --tpmstate dir="$dir" --server type=tcp,port="$port" \
    --ctrl type=tcp,port=$((port + 1)) --flags not-need-init,startup-clear &
  local pid=$!
  export TPM2TOOLS_TCTI="swtpm:host=127.0.0.1,port=$port"
  for _ in $(seq 50); do tpm2_getcap properties-fixed >/dev/null 2>&1 && break; sleep 0.1; done
  # no_da: the index has an empty authValue, so dictionary-attack protection
  # adds nothing but can lock reads out (TPM_RC_LOCKOUT 0x921) after an
  # unclean shutdown, which would silently drop the boot to CURRENT-only.
  tpm2_nvdefine "$TPM_INDEX" -C o -s 8 -a "nt=counter|ownerwrite|authread|ownerread|no_da" >/dev/null
  read_counter() { tpm2_nvread "$TPM_INDEX" -C o -s 8 2>/dev/null | python3 -c 'import sys;print(int.from_bytes(sys.stdin.buffer.read(),"big"))'; }
  tpm2_nvincrement "$TPM_INDEX" -C o
  cur=$(read_counter)
  while [ "$cur" -lt "$want" ]; do tpm2_nvincrement "$TPM_INDEX" -C o; cur=$(read_counter); done
  tpm2_shutdown >/dev/null
  unset TPM2TOOLS_TCTI
  kill "$pid"; wait "$pid" 2>/dev/null || true
  [ "$cur" -eq "$want" ] || { echo "ERROR: fresh TPM counter starts at $cur, cannot reach $want" >&2; exit 1; }
}

FAIL=0
# boot NAME DISK SB(1|0) TPM_DIR|none ; uses EXPECT / FORBID arrays
boot() {
  local name=$1 disk=$2 sb=$3 tpm=$4
  local log="$OUT/$name-serial.log" clean="$OUT/$name-serial.txt" code vars tpid=""
  if [ "$sb" = 1 ]; then code=$OVMF_SB_CODE; vars=$OVMF_SB_VARS; else code=$OVMF_PLAIN_CODE; vars=$OVMF_PLAIN_VARS; fi
  cp "$vars" "$OUT/$name-vars.fd"
  local q=(qemu-system-x86_64 -machine q35,smm=on -global driver=cfi.pflash01,property=secure,value=on
    -m 512M -smp 2 -cpu max "${ACCEL[@]}" -display none -monitor none -serial "file:$log" -no-reboot
    -drive "if=pflash,format=raw,readonly=on,file=$code" -drive "if=pflash,format=raw,file=$OUT/$name-vars.fd"
    -drive "if=none,id=disk,format=raw,file=$disk" -device ide-hd,drive=disk,bus=ide.0,bootindex=1)
  if [ "$tpm" != none ]; then
    swtpm socket --tpm2 --tpmstate dir="$tpm" --ctrl type=unixio,path="$tpm/ctrl.sock" &
    tpid=$!
    for _ in $(seq 50); do [ -S "$tpm/ctrl.sock" ] && break; sleep 0.1; done
    q+=(-chardev "socket,id=chrtpm,path=$tpm/ctrl.sock" -tpmdev emulator,id=tpm0,chardev=chrtpm -device tpm-tis,tpmdev=tpm0)
  fi
  echo "== boot: $name"
  timeout "$TIMEOUT" "${q[@]}" &
  local qpid=$! last=${EXPECT[${#EXPECT[@]}-1]}
  for _ in $(seq 1 "$TIMEOUT"); do
    grep -aqF -- "$last" "$log" 2>/dev/null && break
    kill -0 "$qpid" 2>/dev/null || break
    sleep 1
  done
  sleep 2
  kill "$qpid" 2>/dev/null || true; wait "$qpid" 2>/dev/null || true
  [ -n "$tpid" ] && { kill "$tpid" 2>/dev/null || true; wait "$tpid" 2>/dev/null || true; rm -f "$tpm/ctrl.sock"; }
  tr -d '\r' < "$log" | sed 's/\x1b\[[0-9;=?]*[A-Za-z]//g' > "$clean"
  local bad=0 m
  for m in "${EXPECT[@]}"; do
    if grep -aqF -- "$m" "$clean"; then echo "  ok: $m"; else echo "  MISSING: $m"; bad=1; fi
  done
  for m in "${FORBID[@]}"; do
    if grep -aqF -- "$m" "$clean"; then echo "  FORBIDDEN: $m"; bad=1; fi
  done
  if [ "$bad" -ne 0 ]; then echo "---- serial ($clean) ----"; tail -60 "$clean"; FAIL=1; fi
}

KERNEL_BANNER='Peregrinus OS — Purgatorio 0.1 Admission Gate'
CTRL='Peregrinus OS Muro 1.0 Stable Trusted Boot Controller'

echo "== TPM provisioning"
for c in 40 41 42 43; do provision_tpm "$OUT/tpm-$c" "$c"; done

# 1. COMMITTED counter, Secure Boot on: the whole chain runs and the kernel
#    confirms the attempt the controller recorded; twice on the same disk.
cp "$OUT/base.img" "$OUT/committed.img"
EXPECT=("$CTRL" 'Secure Boot: ACTIVE' 'TPM state: COMMITTED; IA_RECOVERY pre-boot journal enabled'
        'Pre-boot Recovery Journal selection: CURRENT' 'Kernel .text SHA-256: PASS' 'Build generation: 23'
        'IA_RECOVERY direct boot-success commit: CONFIRMED')
FORBID=('PEREGRINUS KERNEL PANIC' 'CPU EXCEPTION' 'FAIL-CLOSED' 'SPLIT-BRAIN')
boot committed-1 "$OUT/committed.img" 1 "$OUT/tpm-42"
boot committed-2 "$OUT/committed.img" 1 "$OUT/tpm-42"

# 2. STALE / FUTURE counter: the controller refuses before any loader runs.
for c in 40 43; do
  cp "$OUT/base.img" "$OUT/counter-$c.img"
  EXPECT=("$CTRL" 'Secure Boot: ACTIVE' 'TPM counter incompatible with media; FAIL-CLOSED')
  FORBID=("$KERNEL_BANNER" 'Pre-boot Recovery Journal selection')
  boot "counter-$c" "$OUT/counter-$c.img" 1 "$OUT/tpm-$c"
done

# 3. Tampered kernel on the ESP: Limine's enrolled-config hash pin stops it.
EXPECT=("$CTRL" 'TPM state: COMMITTED' 'Pre-boot Recovery Journal selection: CURRENT')
FORBID=("$KERNEL_BANNER")
cp -r "$OUT/tpm-42" "$OUT/tpm-42-tamper"
boot tampered-kernel "$OUT/tampered.img" 1 "$OUT/tpm-42-tamper"

# 4. Unsigned controller under Secure Boot: firmware never starts it.
EXPECT=('Access Denied')
FORBID=("$CTRL" "$KERNEL_BANNER")
boot unsigned-controller "$OUT/unsigned.img" 1 "$OUT/tpm-42"

if [ "$FAIL" -ne 0 ]; then echo "FAIL: trusted chain"; exit 1; fi
echo "PASS: trusted chain"
