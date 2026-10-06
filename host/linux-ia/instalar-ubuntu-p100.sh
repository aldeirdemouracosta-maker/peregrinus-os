#!/usr/bin/env bash
# Prepara um Ubuntu 24.04 (desktop ou server) para servir IA na NVIDIA Tesla P100 (CUDA) ao
# Peregrinus OS. Rode como usuário comum com sudo; o script pede confirmação e pode ser rodado
# de novo (cada etapa pula o que já está feito). Guia completo: docs/IA-GPU.md.
#
#   1. confere o sistema e a placa (P100 = Pascal, sm_60)
#   2. driver NVIDIA 580 proprietário (a última série com Pascal; os módulos "-open" não servem)
#   3. CUDA 12.9 do repositório da NVIDIA (o CUDA 13 não compila mais para Pascal)
#   4. compila o llama.cpp com CUDA para sm_60 em /opt/llama.cpp
#   5. instala o serviço llama-server (só em 127.0.0.1) e o QEMU para o Peregrinus
#
# Variáveis: LLAMA_CPP_REF (tag/commit do llama.cpp; padrão: master — recomendado fixar),
#            FORCE=1 (pula as checagens de sistema e de placa, por sua conta).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
LLAMA_CPP_REF="${LLAMA_CPP_REF:-master}"
CUDA_VER=12-9
CUDA_HOME=/usr/local/cuda-12.9
PREFIX=/opt/llama.cpp
MODELS=/opt/peregrinus-ia/modelos
CONF=/etc/peregrinus-ia.conf

say(){ printf '\n== %s\n' "$*"; }
die(){ printf 'ERRO: %s\n' "$*" >&2; exit 1; }
confirm(){ read -r -p "$1 [s/N] " r; [[ "$r" == [sS]* ]]; }

say "1/5 Conferindo sistema e placa"
[[ $EUID -ne 0 ]] || die "rode como usuário comum (o script usa sudo quando precisa)"
. /etc/os-release
if [[ "${ID:-}" != ubuntu || "${VERSION_ID:-}" != 24.04 ]] && [[ "${FORCE:-0}" != 1 ]]; then
  die "feito para Ubuntu 24.04 (encontrado: ${PRETTY_NAME:-?}). Use FORCE=1 para tentar mesmo assim."
fi
if ! lspci -nn | grep -qiE '10de:15f[789]'; then
  [[ "${FORCE:-0}" == 1 ]] || die "Tesla P100 não encontrada no lspci (IDs 10de:15f7/15f8/15f9). Confira encaixe, energia (conector EPS 8 pinos) e 'Above 4G Decoding' na BIOS."
fi
lspci -nn | grep -iE '10de:' || true

say "2/5 Driver NVIDIA (série 580, proprietário)"
if nvidia-smi >/dev/null 2>&1; then
  nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv
else
  echo "Será instalado nvidia-driver-580-server (proprietário). Os módulos 'open' da NVIDIA não suportam Pascal."
  confirm "Instalar o driver agora?" || die "cancelado"
  sudo apt-get update
  sudo apt-get install -y nvidia-driver-580-server || sudo apt-get install -y nvidia-driver-580
  echo
  echo "Driver instalado. REINICIE o computador e rode este script de novo."
  echo "Se o Secure Boot estiver ligado, o Ubuntu pede uma senha (MOK) agora e de novo na reinicialização."
  exit 0
fi

say "3/5 CUDA ${CUDA_VER/-/.} (toolkit apenas; o driver já está instalado)"
if [[ ! -x "$CUDA_HOME/bin/nvcc" ]]; then
  confirm "Adicionar o repositório CUDA da NVIDIA e instalar cuda-toolkit-$CUDA_VER?" || die "cancelado"
  tmp=$(mktemp -d)
  wget -q -O "$tmp/cuda-keyring.deb" https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2404/x86_64/cuda-keyring_1.1-1_all.deb
  sudo dpkg -i "$tmp/cuda-keyring.deb"; rm -rf "$tmp"
  sudo apt-get update
  # Só o toolkit: o pacote "cuda" completo tentaria trocar o driver.
  sudo apt-get install -y "cuda-toolkit-$CUDA_VER"
fi
"$CUDA_HOME/bin/nvcc" --version | tail -2

say "4/5 llama.cpp com CUDA para a P100 (sm_60) em $PREFIX"
sudo apt-get install -y build-essential cmake git python3 pciutils
if [[ ! -x "$PREFIX/bin/llama-server" ]] || confirm "llama.cpp já instalado. Recompilar ($LLAMA_CPP_REF)?"; then
  src=$(mktemp -d)
  git clone --filter=blob:none https://github.com/ggml-org/llama.cpp "$src/llama.cpp"
  git -C "$src/llama.cpp" checkout --quiet "$LLAMA_CPP_REF"
  commit=$(git -C "$src/llama.cpp" rev-parse HEAD)
  cmake -S "$src/llama.cpp" -B "$src/build" -DCMAKE_BUILD_TYPE=Release -DGGML_CUDA=ON \
    -DCMAKE_CUDA_ARCHITECTURES=60 -DCMAKE_CUDA_COMPILER="$CUDA_HOME/bin/nvcc" -DLLAMA_CURL=OFF \
    -DBUILD_SHARED_LIBS=OFF
  cmake --build "$src/build" -j"$(nproc)" --target llama-server llama-cli llama-bench
  # Static build: three self-contained binaries (they still load the NVIDIA/CUDA runtime libraries).
  sudo install -d -m 755 "$PREFIX/bin"
  sudo install -m 755 "$src/build/bin/llama-server" "$src/build/bin/llama-cli" "$src/build/bin/llama-bench" "$PREFIX/bin/"
  echo "$commit" | sudo tee "$PREFIX/COMMIT" >/dev/null
  rm -rf "$src"
  echo "llama.cpp $commit instalado em $PREFIX (anote o commit para repetir o mesmo build)."
fi

say "5/5 Serviço llama-server e QEMU"
sudo apt-get install -y qemu-system-x86 ovmf
sudo usermod -aG kvm "$USER" || true
sudo install -d -m 755 "$MODELS"
if [[ ! -f "$CONF" ]]; then
  sudo install -m 644 "$HERE/peregrinus-ia.conf" "$CONF"
fi
sudo install -m 644 "$HERE/llama-server.service" /etc/systemd/system/llama-server.service
sudo systemctl daemon-reload
. "$CONF"
if [[ -f "$MODELO" ]]; then
  sudo systemctl enable --now llama-server
  echo "llama-server ativo: $(systemctl is-active llama-server)"
else
  echo "Nenhum modelo em $MODELO ainda. Baixe um GGUF (veja docs/IA-GPU.md), por exemplo:"
  echo "  sudo wget -O $MODELS/modelo.gguf <URL do GGUF escolhido>"
  echo "  sha256sum $MODELS/modelo.gguf      # anote; compare com o publicado"
  echo "  sudo systemctl enable --now llama-server"
fi
echo
echo "Pronto. Para usar: host/linux-ia/iniciar-peregrinus-ia.sh <ISO ia-ponte>"
echo "(Saia e entre de novo na sessão para o grupo 'kvm' valer.)"
