#!/usr/bin/env bash
# Expose the versioned LLVM 18 tools under the unversioned names the build uses.
set -euo pipefail
sudo ln -sf /usr/bin/clang-18 /usr/local/bin/clang
sudo ln -sf /usr/bin/clang++-18 /usr/local/bin/clang++
sudo ln -sf /usr/bin/ld.lld-18 /usr/local/bin/ld.lld
sudo ln -sf /usr/bin/lld-link-18 /usr/local/bin/lld-link
for t in objcopy objdump nm readelf; do sudo ln -sf "/usr/bin/llvm-$t-18" "/usr/local/bin/llvm-$t"; done
