#!/usr/bin/env bash
# Abre o Peregrinus OS (perfil ia-ponte) numa VM e liga a ponte de IA ao llama-server local.
# A janela do QEMU mostra a tela do Peregrinus (digite lá); este terminal mostra o log e também
# aceita comandos para o shell. Ex.: pergunte Qual é a capital do Brasil?
#
# Uso: iniciar-peregrinus-ia.sh <peregrinus-...-ia-ponte.iso>
# Variáveis: IA_SERVIDOR (padrão http://127.0.0.1:8080), PEREGRINUS_DISPLAY (padrão: janela;
#            "none" = sem janela), PEREGRINUS_RAM (padrão 256M).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ISO="${1:-$ROOT/build-ia-ponte/peregrinus-purgatorio-0.1.1-ia-ponte.iso}"
SERVER="${IA_SERVIDOR:-http://127.0.0.1:8080}"
SOCK="${XDG_RUNTIME_DIR:-/tmp}/peregrinus-serial-$$.sock"
[[ -f "$ISO" ]] || { echo "ISO não encontrada: $ISO (gere com: ./scripts/make-iso.sh ia-ponte)" >&2; exit 1; }
command -v qemu-system-x86_64 >/dev/null || { echo "Instale o QEMU: sudo apt install qemu-system-x86" >&2; exit 1; }

if python3 - "$SERVER" <<'PY'
import sys, urllib.request
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
import urllib.error
try: opener.open(sys.argv[1] + "/health", timeout=3)
except urllib.error.HTTPError: pass          # the server answered: it is up
except Exception: sys.exit(1)
PY
then echo "IA ok em $SERVER"
else echo "Aviso: o servidor de IA em $SERVER não respondeu (sudo systemctl status llama-server). O Peregrinus abre mesmo assim."
fi

ACCEL=(-cpu max)
if [[ -w /dev/kvm ]]; then ACCEL=(-enable-kvm -cpu host); fi
DISPLAY_ARGS=()
[[ -n "${PEREGRINUS_DISPLAY:-}" ]] && DISPLAY_ARGS=(-display "$PEREGRINUS_DISPLAY")
rm -f "$SOCK"
qemu-system-x86_64 -machine q35 -m "${PEREGRINUS_RAM:-256M}" "${ACCEL[@]}" "${DISPLAY_ARGS[@]}" -no-reboot \
  -cdrom "$ISO" -chardev socket,id=s0,path="$SOCK",server=on,wait=off -serial chardev:s0 &
QEMU_PID=$!
cleanup(){ kill "$QEMU_PID" 2>/dev/null || true; rm -f "$SOCK"; }
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM HUP
STDIN=()  # o teclado deste terminal vai para o shell do Peregrinus
python3 "$ROOT/scripts/ia-ponte.py" --serial "unix:$SOCK" --server "$SERVER" "${STDIN[@]}"
