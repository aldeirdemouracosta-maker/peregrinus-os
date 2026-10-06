#!/usr/bin/env bash
# Regenerate SOURCE_SHA256SUMS from the files tracked by git (new files included).
# --check: fail if the committed list is stale (missing files, extra files or wrong hashes).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
list(){ git ls-files --cached --others --exclude-standard | grep -vE '^(artifacts/|\.github/|SOURCE_SHA256SUMS$|CLAUDE\.md$|REVIEW\.md$|\.gitignore$)' | while read -r f; do [[ -f "$f" ]] && echo "./$f"; done | LC_ALL=C sort; }
if [[ "${1:-}" == "--check" ]]; then
  diff -u <(awk '{print $2}' SOURCE_SHA256SUMS | LC_ALL=C sort) <(list) || { echo 'FAIL: SOURCE_SHA256SUMS file list is stale; run scripts/update-source-sums.sh' >&2; exit 1; }
  sha256sum -c SOURCE_SHA256SUMS --quiet
  echo 'PASS: SOURCE_SHA256SUMS covers every tracked source file and matches.'
else
  list | xargs sha256sum > SOURCE_SHA256SUMS
  echo "SOURCE_SHA256SUMS: $(wc -l < SOURCE_SHA256SUMS) files"
fi
