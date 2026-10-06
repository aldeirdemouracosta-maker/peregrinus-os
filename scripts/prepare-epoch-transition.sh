#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
P="$ROOT/scripts/release-profile.py"
CG="$($P current_generation)"; LG="$($P lkg_generation)"; CE="$($P current_epoch)"; LE="$($P lkg_epoch)"; MIN="$($P min_epoch)"
if (( CE <= LE )); then
  echo "SAFE STOP: current release profile is same-epoch (CURRENT=$CG/epoch$CE, LKG=$LG/epoch$LE)." >&2
  echo "No TPM epoch transition is required or permitted for this release." >&2
  exit 3
fi
BASE="${1:-${PEREGRINUS_TPM_COUNTER_BASE:-}}"
[[ -n "$BASE" && "$BASE" =~ ^[0-9]+$ ]] || { echo "Usage: $0 <current-absolute-tpm-counter>" >&2; exit 2; }
(( BASE > 0 )) || { echo "ERROR: counter must be initialized (>0)" >&2; exit 2; }
NEXT=$((BASE+1))
cd "$ROOT"
make secure-slots
rm -rf build-bootctl-epoch
EXTRA_CFLAGS="-DPEREGRINUS_TPM_COMMIT_BASE=${BASE}ull -DPEREGRINUS_TPM_COMMIT_NEXT=${NEXT}ull -DPEREGRINUS_PRECOMMIT_MIN_EPOCH=${LE}ull -DPEREGRINUS_POSTCOMMIT_MIN_EPOCH=${MIN}ull" ./scripts/build-trusted-boot-controller.sh build-bootctl-epoch
EFI_SRC=""; for f in third_party/limine-dist/bin/BOOTX64.EFI; do [[ -f "$f" ]] && { EFI_SRC="$f"; break; }; done
[[ -n "$EFI_SRC" ]] || { echo 'ERROR: BOOTX64.EFI not found; run scripts/fetch-limine.sh.' >&2; exit 1; }
[[ -x third_party/limine-dist/bin/limine ]] || { echo 'ERROR: Limine host tool not built.' >&2; exit 1; }
rm -rf build-transition; mkdir -p build-transition/EFI/Peregrinus/staged build-transition/EFI/Peregrinus/committed build-transition/boot
cp build-secure/boot/peregrinus-current.elf build-transition/boot/; cp build-secure/boot/peregrinus-lkg.elf build-transition/boot/
cp build-secure/boot/limine/limine-staged.conf build-transition/EFI/Peregrinus/staged/limine.conf; cp build-secure/boot/limine/limine-committed.conf build-transition/EFI/Peregrinus/committed/limine.conf
cp "$EFI_SRC" build-transition/EFI/Peregrinus/staged/limine_x64.efi; cp "$EFI_SRC" build-transition/EFI/Peregrinus/committed/limine_x64.efi; cp build-bootctl-epoch/peregrinus-boot-controller.efi build-transition/EFI/Peregrinus/peregrinus-boot-controller.efi
hash_file(){ python3 - "$1" <<'PYINNER'
import hashlib,sys
print(hashlib.blake2b(open(sys.argv[1],'rb').read()).hexdigest())
PYINNER
}
SH=$(hash_file build-transition/EFI/Peregrinus/staged/limine.conf); CH=$(hash_file build-transition/EFI/Peregrinus/committed/limine.conf)
third_party/limine-dist/bin/limine enroll-config build-transition/EFI/Peregrinus/staged/limine_x64.efi "$SH"; third_party/limine-dist/bin/limine enroll-config build-transition/EFI/Peregrinus/committed/limine_x64.efi "$CH"
cat > build-transition/EPOCH-COMMIT-PLAN.txt <<PLAN
PEREGRINUS_EPOCH_COMMIT_PLAN=1
TPM_NV_INDEX=0x0180F050
TPM_COUNTER_BASE=$BASE
TPM_COUNTER_NEXT=$NEXT
PRECOMMIT_MIN_SECURITY_EPOCH=$LE
POSTCOMMIT_MIN_SECURITY_EPOCH=$MIN
CURRENT_GENERATION=$CG
CURRENT_SECURITY_EPOCH=$CE
LKG_GENERATION=$LG
LKG_SECURITY_EPOCH=$LE
STAGED_CONFIG_BLAKE2B=$SH
COMMITTED_CONFIG_BLAKE2B=$CH
PLAN
echo "PASS: prepared cross-epoch transition $LG/epoch$LE -> $CG/epoch$CE; TPM $BASE -> $NEXT"
