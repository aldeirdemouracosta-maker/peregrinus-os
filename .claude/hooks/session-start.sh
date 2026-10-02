#!/bin/bash
# Installs the toolchain needed for `make check` and `make qemu-boot-test` in
# Claude Code cloud sessions. Idempotent; skips packages already present.
set -euo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

need=()
for pair in clang:clang lld:ld.lld llvm:llvm-objcopy python3:python3 make:make \
            qemu-system-x86:qemu-system-x86_64 xorriso:xorriso nasm:nasm mtools:mcopy \
            dosfstools:mkfs.fat swtpm:swtpm tpm2-tools:tpm2_nvdefine sbsigntool:sbsign; do
  pkg=${pair%%:*}; bin=${pair#*:}
  command -v "$bin" >/dev/null 2>&1 || need+=("$pkg")
done
[ -f /usr/share/OVMF/OVMF_CODE_4M.fd ] || need+=(ovmf)

if [ ${#need[@]} -gt 0 ]; then
  SUDO=""; [ "$(id -u)" -eq 0 ] || SUDO=sudo
  $SUDO apt-get update -qq
  DEBIAN_FRONTEND=noninteractive $SUDO apt-get install -y -qq --no-install-recommends "${need[@]}" >/dev/null
fi

# Pinned Limine (SHA-256 verified); cached in third_party/limine-bin.
cd "$CLAUDE_PROJECT_DIR"
./scripts/fetch-limine.sh >/dev/null
