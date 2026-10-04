# Purgatório 0.1.1 — correção e qualificação em runtime

Geração 24, security epoch 3. Até a 0.1 os perfis só tinham testes host-side. Na 0.1.1 todos
os perfis passaram a bootar no QEMU, e isso expôs erros que nenhum teste pegava. Toda
afirmação abaixo é verificada por `make check` ou por `scripts/qemu-qualify.sh`, que roda na CI.

## Erros corrigidos

| # | Erro | Efeito antes | Correção | Prova |
|---|---|---|---|---|
| 1 | `boot_policy` devolvia `none` sem disco e o `guard_policy` convertia em `HALT-FAIL-CLOSED` | o perfil e1000 parava **antes** de ligar a placa; o datapath Muro 0.3–1.0 nunca executou | ação `BOOT-PASSIVE` só para builds **sem** caminho de armazenamento (`features::storage_policy_required`); builds com disco continuam fail-closed | `tests/guard_policy.sh`; QEMU `e1000`, `safe_bios`, `safe_uefi` |
| 2 | sonda GPT exigia `disposable_qemu_disk_only`, falso no recovery-live | o perfil recovery-live **sempre** entrava em pânico ("success commit failed") | sonda depende só de `ahci_dma_read_live`; `static_assert` impede perfil de commit sem leitura de GPT/journal | `tests/profile_matrix.sh`; QEMU `recovery_live` (commit gravado e conferido no disco) |
| 3 | `while(spins--)` + `if(!spins)`: o contador dá a volta | comando AHCI que expirou (inclusive **escrita**) era reportado como sucesso | `spin::until` (orçamento que expira de verdade); timeout/erro após o doorbell **envenena** o controlador | `tests/ahci_timeout.sh` (HBA emulado) |
| 4 | envio e1000: TDT já entregue, timeout não avançava o software | anel TX dessincronizado | timeout desliga a NIC (`ready=false`, RX/TX off, bus master off) | `tests/e1000_driver.sh` |
| 5 | bus mastering ligado antes do reset da NIC | DMA residual de firmware/PXE | bus master desligado → reset (CTRL.RST, espera limitada) → anéis → bus master | `tests/e1000_driver.sh` |
| 6 | Makefile sem dependência de headers | objetos com layout antigo de struct linkados (pânico intermitente) | `-MMD -MP` | build limpo + QEMU |
| 7 | limitador de rajada não cobria ICMP e era contornável trocando IP de origem | 200/200 pings respondidos | echo request entra no limitador; teto agregado por janela | `tests/muro_remaining_stages.sh`; QEMU `e1000` (32/200) |
| 8 | Purgatório era lista de bloqueio (`admit` ignorava o digest) | componente desconhecido era admitido | lista de permissão: `trust(id, digest)`; desconhecido ou digest errado → negado | `tests/quarantine.sh` |
| 9 | ACPI só lia XSDT | MADT/MCFG ignorados em firmware ACPI 1.0 (SeaBIOS) | fallback para RSDT | `tests/acpi_tables.sh`; QEMU `safe_bios` |
| 10 | resposta ICMP copiava o TTL do pedido; ARP/ICMP respondiam a origens de grupo/zero/próprio IP | reflexão / respostas inválidas | TTL 64; origens não-unicast recusadas | self-tests ARP/ICMP; QEMU `e1000` (pedido com TTL 5) |
| 11 | `fetch-limine.sh` clonava sobre diretório existente; `run-qemu.sh` não achava OVMF do Ubuntu 24.04 e usava nome de disco errado | ferramentas quebradas | tarball fixado por SHA-256 em `third_party/limine-dist/`; OVMF por `-bios`/pflash; nomes derivados do release profile | CI `qemu` |
| 12 | `.efi` não reproduzível (timestamp do lld-link) e artefatos commitados não batiam com o build | binários não verificáveis | `/Brepro`; `scripts/make-artifacts.sh`; CI compara bit a bit | CI `reproducible` |

## Endurecimento

- `-fstack-protector-strong` no kernel (canário global, semente RDRAND/TSC) e no controlador UEFI (cookie /GS, semente antes de qualquer frame protegido).
- Pilha do kernel de 64 KiB com página de guarda (PML4 slot 509); TSS com IST para #DF, NMI e #MC. QEMU `double_fault`: estouro proposital → "Vector: 8" reportado, sem triple fault.
- Laço do UART com orçamento; `mmio::map` desfaz mapeamentos parciais; descritores RX lidos via `volatile` uma única vez.
- Mensagens honestas: selo `.text` descrito como verificação de corrupção (não assinado); slot "compilado, não atestado"; epoch verificada em tempo de compilação.

## Excessos removidos

- 10 flags de `features.hpp` que nenhum código lia (e que `safety.sh` "testava").
- Classe `Hook` (só usada em testes) e o anel de auditoria do `firewall::Engine`: a auditoria agora acontece num único ponto, o `net::Datapath`.
- Scripts de teste unificados em `tests/host-cxx.sh`: um compilador, `-Werror` em todos.

## Ferramentas novas

- `scripts/qemu-qualify.sh`: SAFE (BIOS e UEFI), controlador UEFI→Limine→kernel, e1000 com tráfego real (`tests/qemu/net_inject.py`), recovery-commit e recovery-live em disco descartável (`tests/qemu/check_journal.py`), estouro de pilha.
- `tests/fuzz.sh`: libFuzzer + ASan/UBSan em todos os parsers de entrada não confiável.
- `tests/profile_matrix.sh`: as flags de cada perfil são verificadas com `static_assert`, e combinações proibidas não compilam.
- CI: `qemu`, `fuzz`, `reproducible` (com attestation de proveniência no `main`), CodeQL e Dependabot.

## Console de texto na tela

O log do boot, inclusive pânico e exceções, aparece no framebuffer e não só na serial. Usa uma
grade estática (no máximo 160×90 células, sem heap) e a fonte 8×8 de domínio público
(`THIRD_PARTY_NOTICES.md`), com glifos 2× a partir de 1024×600, e maiores (até 4×) em telas grandes, em vez de mais células. Rola meia tela por vez,
para limitar as escritas no framebuffer. Mostra `?` para caracteres fora do ASCII. Sem framebuffer
utilizável, o console fica desligado e a serial continua (fail-closed). Testes:
`tests/text_console.sh` e o cenário QEMU `screen`, que lê o texto direto dos pixels da tela.

## Teclado e shell

Depois do boot o sistema abre o prompt `peregrinus>` em vez de parar. A entrada vem do teclado
PS/2 (i8042 por polling, scancode set 1, layout US) e também da COM1, o que permite operar o
sistema por um cabo serial ou por outro programa. Comandos: `ajuda`, `sobre`, `status`, `hw`,
`limpar`, `reiniciar`, `parar` (com aliases em inglês); nenhum grava nada. A linha é limitada a
96 caracteres e aceita só ASCII. Sem controlador PS/2, o shell segue pela serial. O console de
texto passou a desenhar acentos e cedilha (U+00A0–U+00FF da mesma fonte de domínio público).
Ainda sem interrupções, então o laço de entrada ocupa a CPU (aceitável em VM; melhorar depois).
Testes: `tests/shell.sh` (decodificador, editor, comandos) e o cenário QEMU `shell`, que digita
pelo teclado emulado (`sendkey`) e pela serial e lê o resultado também nos pixels da tela.

## Ressalvas (não resolvidas por código)

- **LKG ainda é a geração 22.** O artefato LKG recovery-live (gen 22) tem o erro 2 e não consegue confirmar boot. Se a CURRENT falhar, um fallback para essa LKG também não confirmará. Promover a geração 24 a LKG é uma decisão de release, a ser tomada depois de qualificar a 0.1.1 em hardware.
- Hardware físico (X79/iTCO, NIC real, TPM real) continua **NOT RUN**.
- O Purgatório ainda não tem chamador: não existe loader de componentes.
- Branch protection e secret scanning são configurações do repositório no GitHub, não código.
