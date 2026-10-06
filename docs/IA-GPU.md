# IA na GPU: Peregrinus OS + Linux + Tesla P100 (perfil `ia-ponte`)

O Peregrinus não usa a GPU. Quem roda a IA é um **Linux ao lado** (o mesmo PC), com o
[llama.cpp](https://github.com/ggml-org/llama.cpp) (MIT) na Tesla P100 via CUDA. O Peregrinus roda
numa VM e conversa com essa IA **pela porta serial**, sem rede e sem TCP.

```
┌──────────────────────── PC com Ubuntu 24.04 ───────────────────────────┐
│  llama-server (CUDA, Tesla P100) ── só 127.0.0.1:8080                  │
│        ▲ HTTP local                                                    │
│  scripts/ia-ponte.py ── terminal: log do Peregrinus + teclado          │
│        ▲ COM1 virtual (socket unix)                                    │
│  QEMU/KVM ─► Peregrinus OS, perfil ia-ponte                            │
│              peregrinus> pergunte Qual é a capital do Brasil?          │
└────────────────────────────────────────────────────────────────────────┘
```

## O que está testado

| Parte | Estado |
|---|---|
| Protocolo do kernel: quadros, respostas velhas/malformadas, saneamento, limite de 2048 bytes, Esc, tempo limite | testado (`tests/ai_bridge.sh`) + fuzzing |
| Comando `pergunte` / `pergunte -n` no shell | testado (`tests/shell.sh`) |
| Ponte no Linux: só servidor local, histórico, saneamento, 2000 bytes, servidor fora do ar | testado (`tests/ia_ponte_bridge.sh`) |
| Kernel real no QEMU + ponte real + servidor de IA **falso**: pergunta pelo teclado, resposta na tela, Esc cancela, quadros nunca aparecem na tela | testado (cenário QEMU `ia_ponte`) |
| `iniciar-peregrinus-ia.sh` com servidor falso (pergunta digitada no terminal do Linux) | testado manualmente, sem GPU |
| Perfil SAFE sem a ponte | testado (`tests/safe_binary_isolation.sh`, `tests/profile_matrix.sh`) |
| Driver 580, CUDA 12.9, llama.cpp na P100, serviço systemd, desempenho | **NOT RUN** (não há GPU no ambiente de desenvolvimento) |

## Hardware: Tesla P100 16 GB

| Ponto | O que fazer |
|---|---|
| Arquitetura | Pascal (GP100), CUDA compute capability **6.0** (`sm_60`), 16 GB HBM2 |
| Sem saída de vídeo | O monitor fica na **RX 580** (ou no vídeo integrado). No Linux, `amdgpu` cuida da tela e o driver NVIDIA só do cálculo. |
| Refrigeração **passiva** | A P100 foi feita para servidor com ventilação forçada. Num PC comum ela precisa de uma ventoinha/duto dedicado soprando no dissipador; sem isso ela esquenta, reduz o clock e pode desligar. Acompanhe com `nvidia-smi -q -d TEMPERATURE`. |
| Energia | Até 250 W por um conector **8 pinos tipo CPU/EPS** (não é o PCIe de 8 pinos). Use o adaptador próprio (2× PCIe 8 pinos → EPS 8 pinos). **Cabo errado pode danificar a placa ou a fonte.** Fonte de qualidade com folga (P100 + RX 580: ~750 W). |
| BIOS da placa-mãe | Ligue **Above 4G Decoding**; sem isso a P100 costuma não aparecer ou o driver não inicia. |
| Driver e CUDA | Série **580** do driver, proprietária (os módulos "open" da NVIDIA não suportam Pascal). **CUDA 12.9**: o CUDA 13 deixou de compilar para Pascal. Confira nas notas de versão da NVIDIA se isso mudou. |

## Memória: 16 GB de VRAM + 32 GB de RAM

O modelo inteiro na VRAM é o que dá velocidade. Tamanhos aproximados de arquivos GGUF:

| Modelo (GGUF) | Arquivo | Na P100 16 GB | Observação |
|---|---|---|---|
| Qwen2.5-7B-Instruct Q8_0 | ~8 GB | inteiro, com folga | rápido; bom em português |
| **Qwen2.5-14B-Instruct Q4_K_M** | ~9 GB | inteiro (**recomendado**) | melhor equilíbrio |
| Qwen2.5-14B-Instruct Q6_K | ~12 GB | inteiro, contexto menor | mais fiel, um pouco mais lento |
| Qwen2.5-32B-Instruct Q4_K_M | ~20 GB | não cabe: parte vai para a RAM (`CAMADAS_GPU` ~40) | os 32 GB de RAM permitem, mas fica bem mais lento |

Os 32 GB de RAM sobram para o Linux, a VM do Peregrinus (256 MB) e um editor de vídeo. Os
tamanhos e as licenças dos modelos não foram verificados neste ambiente; confira no cartão de
cada modelo e **anote o SHA-256** do arquivo baixado. Velocidade: **não medida**. Meça na sua
máquina com `llama-bench` (abaixo).

## Instalação

1. **Ubuntu 24.04** (desktop ou server) instalado, monitor ligado na RX 580/vídeo integrado.
2. No repositório do Peregrinus:
   ```sh
   ./host/linux-ia/instalar-ubuntu-p100.sh     # instala o driver 580 e pede para reiniciar
   sudo reboot
   ./host/linux-ia/instalar-ubuntu-p100.sh     # 2ª vez: CUDA 12.9, llama.cpp (sm_60), serviço, QEMU
   ```
   Para repetir exatamente o mesmo build, fixe o llama.cpp: `LLAMA_CPP_REF=<commit> ./host/...`.
   O commit usado fica em `/opt/llama.cpp/COMMIT`.
3. **Modelo:** baixe um GGUF da tabela acima para `/opt/peregrinus-ia/modelos/modelo.gguf`
   (ou ajuste `MODELO=` em `/etc/peregrinus-ia.conf`), confira o SHA-256 e ligue o serviço:
   ```sh
   sha256sum /opt/peregrinus-ia/modelos/modelo.gguf
   sudo systemctl enable --now llama-server
   curl -s http://127.0.0.1:8080/health          # {"status":"ok"} quando o modelo carregou
   nvidia-smi                                     # llama-server deve aparecer usando a VRAM
   /opt/llama.cpp/bin/llama-bench -m /opt/peregrinus-ia/modelos/modelo.gguf -ngl 99   # velocidade
   ```
4. **Peregrinus com a ponte:**
   ```sh
   ./scripts/fetch-limine.sh
   ./scripts/make-iso.sh ia-ponte                 # build-ia-ponte/peregrinus-purgatorio-0.1.1-ia-ponte.iso
   ./host/linux-ia/iniciar-peregrinus-ia.sh
   ```
   Abre a janela do Peregrinus (QEMU com KVM). O terminal mostra o log e aceita comandos também.

## Usar

```
peregrinus> pergunte Qual é a capital do Brasil?
aguardando a IA do Linux... Esc cancela
A capital do Brasil é Brasília.
peregrinus> pergunte E a população?          (continua a mesma conversa)
peregrinus> pergunte -n Outro assunto...      (-n começa uma conversa nova)
```

- A pergunta tem até 160 caracteres (o limite da linha do shell).
- A resposta tem até 2048 bytes. Acentos aparecem normalmente; símbolos que a fonte não tem viram
  um equivalente simples (aspas tipográficas → `"`) ou `?`.
- **Esc** cancela a espera. Sem resposta em **120 s**, aparece "IA indisponível".
- A ponte lembra as últimas 6 trocas da conversa (`--history`).

## Segurança (fail-closed)

- O llama-server escuta só em `127.0.0.1`, e o serviço systemd ainda bloqueia qualquer endereço
  não local (`IPAddressDeny=any`). Ele roda sem privilégios (`DynamicUser`).
- A ponte recusa servidor que não seja local (`127.0.0.1`, `::1`, `localhost`) e ignora proxies.
- A resposta é **saneada duas vezes** (na ponte e no kernel): sem caracteres de controle, sem
  sequências de escape, sem controles C1, UTF-8 inválido vira `?`, tamanho limitado.
- O kernel só **imprime** a resposta. Nada do que a IA diz é executado como comando.
- O perfil `ia-ponte` é o SAFE mais o comando `pergunte`: sem rede, sem disco, sem DMA. Ele não se
  combina com nenhum outro perfil (`kernel/config/features.hpp`), e o SAFE não contém a ponte.
- A IA pode errar. Trate a resposta como sugestão.

## Editor de vídeo profissional no mesmo Linux

O Peregrinus não edita vídeo. O editor roda no Linux, ao lado, usando a mesma P100.
**Nada desta seção foi testado** (NOT RUN).

| Editor | Licença | Na P100 |
|---|---|---|
| **DaVinci Resolve** (gratuito) | proprietário (Blackmagic) | edição, cor e áudio profissionais; usa CUDA para o processamento. No Linux, a versão gratuita **não decodifica nem codifica H.264/H.265 por hardware** e não tem AAC: converta antes para DNxHR (abaixo) ou use a versão Studio (paga). Confira nas notas da versão se a sua versão ainda aceita Pascal. |
| **Kdenlive** | GPL (código aberto) | leve e estável; usa FFmpeg. Exportação por NVENC só se a P100 oferecer o codificador: confira com `ffmpeg -encoders \| grep nvenc` e `nvidia-smi -q \| grep -i -A3 encoder`. |
| **Blender** (Video Sequencer) | GPL | edição simples e efeitos 3D com CUDA. |

Converter um vídeo do celular para editar no Resolve gratuito:

```sh
ffmpeg -i entrada.mp4 -c:v dnxhd -profile:v dnxhr_hq -pix_fmt yuv422p -c:a pcm_s16le saida.mov
```

- **Tela:** o monitor fica na RX 580, e o Resolve processa na P100. Se ele não achar a P100, escolha
  CUDA em *Preferences → Memory and GPU*.
- **VRAM compartilhada:** a IA e o editor disputam os mesmos 16 GB. Para editar, pare a IA
  (`sudo systemctl stop llama-server`), ou use um modelo 7B, que deixa ~8 GB livres.
- **RAM:** 32 GB atendem bem a edição em 1080p e em 4K com arquivos *proxy*.

## Arquivos

| Arquivo | Função |
|---|---|
| `kernel/shell/ai_bridge.{hpp,cpp}` | protocolo, saneamento e espera limitada (kernel) |
| `scripts/ia-ponte.py` | ponte serial ⇄ llama-server (Linux, só biblioteca padrão do Python) |
| `host/linux-ia/instalar-ubuntu-p100.sh` | driver 580, CUDA 12.9, llama.cpp sm_60, serviço, QEMU |
| `host/linux-ia/llama-server.service`, `peregrinus-ia.conf` | serviço systemd endurecido e sua configuração |
| `host/linux-ia/iniciar-peregrinus-ia.sh` | abre a VM e liga a ponte |
| `tests/ai_bridge*.{sh,cpp}`, `tests/ia_ponte_*`, `tests/qemu/ia_ponte_drive.py` | testes |
