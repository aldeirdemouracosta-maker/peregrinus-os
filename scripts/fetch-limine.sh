#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
VERSION=v12.9.1
mkdir -p third_party
if [ ! -d third_party/limine/.git ]; then
  git clone --depth 1 --branch "$VERSION" https://github.com/limine-bootloader/limine.git third_party/limine
fi
printf '%s\n' "Limine pinned to $VERSION. Do not replace with an unpinned branch in release builds."
