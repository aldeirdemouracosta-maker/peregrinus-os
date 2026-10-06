#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
python3 "$ROOT/tests/ia_ponte_bridge_test.py"
echo 'PASS: host AI bridge (loopback-only server, framing, history, sanitizing, 2000-byte cap, server-down error).'
