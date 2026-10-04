#!/usr/bin/env bash
# The kernel's local-LLM engine must produce byte-identical output to llama2.c's reference
# run.c (vendored in tests/llm/run.c), with both using the kernel's math functions. Also checks
# that malformed model/tokenizer files are rejected.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/host-cxx.sh"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
FP="-O2 -msse2 -ffp-contract=off"
python3 "$ROOT/scripts/make-test-model.py" "$TMP/m" >/dev/null
# Reference: run.c unchanged, with libm calls routed to the kernel math.
awk 'BEGIN{done=0} {print} /#endif/ && !done {print "#include \"math_override.h\""; done=1}' "$ROOT/tests/llm/run.c" > "$TMP/run_ref.c"
"$HOST_CC" $FP -w -I"$ROOT/tests/llm" -c "$TMP/run_ref.c" -o "$TMP/run_ref.o"
"$HOST_CXX" -std=c++23 $FP -c "$ROOT/kernel/llm/llm_math.cpp" -o "$TMP/llm_math.o"
"$HOST_CXX" "$TMP/run_ref.o" "$TMP/llm_math.o" -o "$TMP/run_ref"
host_cxx $FP "$ROOT/tests/llm_test.cpp" "$ROOT/kernel/llm/engine.cpp" "$ROOT/kernel/llm/llm_math.cpp" -o "$TMP/engine"
check(){  # temperature topp seed steps prompt
  "$TMP/run_ref" "$TMP/m/model.bin" -z "$TMP/m/tokenizer.bin" -t "$1" -p "$2" -s "$3" -n "$4" -i "$5" > "$TMP/ref.txt" 2>/dev/null
  "$TMP/engine" "$TMP/m/model.bin" "$TMP/m/tokenizer.bin" "$1" "$2" "$3" "$4" "$5" > "$TMP/eng.txt"
  if ! cmp -s "$TMP/ref.txt" "$TMP/eng.txt"; then
    echo "FAIL: engine differs from run.c (t=$1 p=$2 s=$3 n=$4 prompt='$5')"; diff <(od -c "$TMP/ref.txt" | head -20) <(od -c "$TMP/eng.txt" | head -20) || true; exit 1
  fi
}
check 0 0.9 1 128 "Peregrinus"
check 0 0.9 1 64 "Olá mundo, sistema micro!"
check 0 0.9 1 40 ""
check 1.0 1.0 42 128 "Peregrinus kernel"
check 0.8 0.9 7 128 "de os as"
"$TMP/engine" "$TMP/m/model.bin" "$TMP/m/tokenizer.bin" 0 0.9 1 128 "Peregrinus" > "$TMP/g.txt"
[[ $(wc -c < "$TMP/g.txt") -gt 20 ]] || { echo "FAIL: suspiciously short generation"; exit 1; }
# Malformed inputs must be rejected (fail-closed), never crash.
head -c 1000 "$TMP/m/model.bin" > "$TMP/short.bin"
{ "$TMP/engine" "$TMP/short.bin" "$TMP/m/tokenizer.bin" 0 0.9 1 8 x || true; } | grep -q "invalid files" || { echo "FAIL: truncated model accepted"; exit 1; }
head -c 2000 "$TMP/m/tokenizer.bin" > "$TMP/short_tok.bin"
{ "$TMP/engine" "$TMP/m/model.bin" "$TMP/short_tok.bin" 0 0.9 1 8 x || true; } | grep -q "invalid files" || { echo "FAIL: truncated tokenizer accepted"; exit 1; }
python3 - "$TMP/m/model.bin" "$TMP/bad.bin" <<'PY'
import struct, sys
d = bytearray(open(sys.argv[1], 'rb').read()); struct.pack_into('<i', d, 12, 3)  # n_heads=3 does not divide dim=64
open(sys.argv[2], 'wb').write(d)
PY
{ "$TMP/engine" "$TMP/bad.bin" "$TMP/m/tokenizer.bin" 0 0.9 1 8 x || true; } | grep -q "invalid files" || { echo "FAIL: bad geometry accepted"; exit 1; }
# The test model is deterministic, so its digests must match the allowlist entry the QEMU test uses.
python3 - "$TMP/m" "$ROOT/include/peregrinus/model_allowlist.h" <<'PY'
import hashlib, sys
d = lambda p: '{' + ', '.join('0x%02x' % b for b in hashlib.sha256(open(p, 'rb').read()).digest()) + '}'
h = open(sys.argv[2]).read()
if d(sys.argv[1] + '/model.bin') not in h or d(sys.argv[1] + '/tokenizer.bin') not in h:
    sys.exit('FAIL: regenerated test model digests are not in model_allowlist.h (generator not deterministic?)')
PY
echo 'PASS: local-LLM engine is byte-identical to llama2.c run.c (greedy, temperature, top-p) and rejects malformed files.'
