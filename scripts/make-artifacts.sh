#!/usr/bin/env bash
# Build every release/qualification binary from source and write them, with checksums and a
# size report, to OUT (default: artifacts/). CI rebuilds into a temporary directory and fails
# if the checksums differ from the committed ones: committed binaries must be reproducible.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
OUT="${1:-artifacts}"
TAG=$(./scripts/release-profile.py tag | tr 'A-Z' 'a-z')
GEN=$(./scripts/release-profile.py current_generation)
P="peregrinus-$TAG"
make -s clean >/dev/null
make -s all >/dev/null
make -s current-recovery-live >/dev/null
make -s qemu-e1000-sandbox >/dev/null
make -s llm-local >/dev/null
./scripts/build-trusted-boot-controller.sh build-bootctl >/dev/null
make -s preboot-recovery-controller >/dev/null
mkdir -p "$OUT"
rm -f "$OUT"/*.elf "$OUT"/*.efi
cp build/peregrinus.elf "$OUT/$P-current-gen$GEN-safe.elf"
cp build-current-recovery-live/peregrinus.elf "$OUT/$P-current-gen$GEN-recovery-live.elf"
cp build-qemu-e1000/peregrinus.elf "$OUT/$P-current-gen$GEN-e1000-qemu.elf"
cp build-llm-local/peregrinus.elf "$OUT/$P-current-gen$GEN-llm-local.elf"
cp build-bootctl/peregrinus-boot-controller.efi "$OUT/$P-boot-controller-safe.efi"
cp build-bootctl-preboot/peregrinus-boot-controller.efi "$OUT/$P-boot-controller-recovery-live.efi"
(cd "$OUT" && sha256sum ./*.elf ./*.efi | sed 's#  \./#  #' > SHA256SUMS && size ./*.elf | sed 's#\./##' > SIZE_REPORT.txt)
echo "artifacts written to $OUT ($(clang --version | head -1))"
