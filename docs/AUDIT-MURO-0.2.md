# Peregrinus OS — Auditoria de enxugamento e melhorias

Baseline auditado: **Muro das Lamentações 0.2 / CURRENT gen12 / LKG gen11 / security epoch 3**.

## Resultado geral

A base está tecnicamente coerente e `make check` passa integralmente, mas o projeto acumulou scaffolding de desenvolvimento e três problemas arquiteturais que devem ser corrigidos antes de adicionar o driver e1000 RX:

1. **Perfil de release fragmentado em várias fontes de verdade.** Há valores antigos de geração/epoch em scripts e headers ativos.
2. **Estado de recuperação duplicado.** Boot Control e Recovery Journal carregam campos e decisões sobrepostas; o kernel e o controlador pre-boot podem consultar fontes diferentes.
3. **Self-tests de desenvolvimento estão dentro do kernel de produção e rodam em todo boot.**

## Métricas observadas

- 212 arquivos no pacote de trabalho.
- ~4.756 linhas C/C++/headers/ASM em kernel + boot controller + testes.
- Kernel: ~3.664 linhas.
- Safe ELF atual: text 50.570 B; data 480 B; BSS 20.832 B; total carregado 71.882 B.
- `.rodata`: ~8,3 KiB.
- Pacote descompactado: ~2,0 MiB; artifacts: ~776 KiB; docs históricos: ~152 KiB; testes: ~172 KiB.
- `--gc-sections` + function/data sections, sem alterar lógica, reduziu o total carregado de 71.882 B para 68.794 B e text de 50.570 B para 47.546 B.
- Símbolos de self-test presentes no kernel somam aproximadamente **10,9 KiB de código**, sem contar helpers/strings associados.

## PRIORIDADE CRÍTICA — corrigir antes de avançar

### 1. Uma única fonte de verdade para release/profile

Há divergências atuais:

- `Makefile`: CURRENT 12 / LKG 11 / epoch 3.
- `kernel/security/root_trust.hpp`: default generation 11 / epoch 3.
- `bootctl/uefi/trusted_boot_controller.c`: defaults CURRENT 11 / LKG 10.
- `bootctl/common/boot_request.h`: minimum epoch 2.
- `bootctl/common/tpm_commit.h`: defaults de transição epoch 2 -> 3 e LKG epoch 2.
- `scripts/create-gpt-test-image.py`: defaults CURRENT 10 / LKG 9.
- `tests/preboot_controller_binary.sh`: compila perfil 10 / 9.
- `scripts/make-boot-request.py`: defaults epoch 2 / generation 2.
- `scripts/prepare-epoch-transition.sh`: ainda grava plano CURRENT gen6 / LKG gen5, epoch 3 / 2.
- `scripts/commit-tpm-epoch.sh`: mensagem de confirmação ainda exige CURRENT generation 6.

**Risco:** um bundle de update/TPM pode ser assinado com metadados incompatíveis. O contador TPM é monotônico e o commit pode ser irreversível.

**Ação:** criar `config/release_profile.*` como fonte única, por exemplo:

- release = MURO-0.2
- current_generation = 12
- lkg_generation = 11
- current_epoch = 3
- lkg_epoch = 3
- minimum_epoch = 3

Gerar automaticamente headers C/C++, Make variables, scripts e configs Limine. Remover defaults silenciosos: build de release deve falhar se o perfil não estiver definido.

**Até isso ser corrigido, não executar `prepare-epoch-transition.sh` ou commit TPM em hardware.**

### 2. Remover Boot Control como terceira fonte de estado operacional

Hoje existem três famílias A/B em `IA_RECOVERY`:

- Recovery Anchor A/B — identidade, geometria, known-good, clean shutdown.
- Boot Control A/B — current/lkg generation/epoch, attempts, confirmed.
- Recovery Journal A/B — current/lkg generation/epoch, attempts, last slot, success.

Boot Control e Recovery Journal se sobrepõem fortemente. O controlador pre-boot já usa Recovery Journal, enquanto o kernel ainda calcula rollback a partir de Boot Control. Isso cria duas autoridades para a mesma decisão.

**Recomendação:**

- manter **Recovery Anchor A/B** como metadado estático/de identidade;
- manter **Recovery Journal A/B** como estado mutável de boot;
- remover **Boot Control A/B**;
- mover a decisão CURRENT/LKG definitivamente para o Trusted Boot Controller;
- no kernel, apenas verificar se o slot em execução corresponde ao journal pendente e falhar fechado se houver inconsistência.

Isso elimina parser, probe, testes e setores duplicados e reduz risco de divergência.

### 3. Tirar self-tests sintéticos do boot normal

O `kmain()` executa em toda inicialização testes sintéticos de:

- root trust;
- SHA-256;
- integrity parser;
- iTCO;
- ATA IDENTIFY;
- GPT;
- recovery anchor;
- recovery journal;
- boot control;
- rollback;
- boot policy;
- watchdog;
- volume catalog;
- guard policy;
- firewall;
- Ethernet/IPv4;
- hook;
- NIC classifier.

Além do ~10,9 KiB de código de self-test, alguns testes usam estruturas grandes na pilha e o watchdog self-test destrói/restaura o estado de sequência em pleno boot.

**Manter no runtime apenas verificações reais:** kernel seal, epoch floor, checkpoints do watchdog, GPT/recovery/journal reais quando habilitados e política fail-closed.

Os testes sintéticos devem ficar em host tests/CI e, opcionalmente, num perfil `PEREGRINUS_DIAGNOSTIC_BOOT=1` separado.

## PRIORIDADE ALTA — enxugamento de RAM/código

### 4. Não reservar DMA32 no SAFE

`memory.cpp` reserva até **2 MiB de DMA32** em todo boot, inclusive quando DMA live está desabilitado. O safe build não precisa dessa reserva.

Reservar DMA32 somente quando um perfil realmente habilitar AHCI DMA, Recovery-Live ou futuro NIC DMA.

### 5. Remover o PMM self-test que perde uma página

`kmain()` chama `alloc_page()` apenas para imprimir um endereço e nunca devolve a página. Como o allocator é bump-only, isso consome **4 KiB permanentemente** a cada boot.

Remover no release; testar o allocator em host/diagnostic build.

### 6. Compactar o catálogo PCI

`pci::Device` ocupa 112 B e `g_devices[96]` consome **10.752 B de BSS**. Quase tudo vem de seis `BarInfo` por dispositivo.

Além do desperdício, o limite 96 cria um problema: um dispositivo relevante enumerado depois dos primeiros 96 pode não ser armazenado.

**Melhor desenho:** guardar um registro PCI compacto (BDF, IDs, classe/subclasse/prog-if/header), e ler BARs sob demanda somente para AHCI/GPU/NIC selecionados. Alternativamente registrar apenas classes/dispositivos de interesse durante a enumeração.

### 7. Evitar duplicação GPT -> Volume Catalog

`gpt::TableInfo` (~1.680 B) e `volume::Catalog` (~1.688 B) mantêm informações muito semelhantes. O Peregrinus atualmente só precisa de SYSTEM/RECOVERY/DATA.

Após validar GPT, condensar para 3 volumes protegidos + metadados mínimos; não manter 16 partições completas em duas estruturas residentes.

### 8. Um único audit ring para o datapath

O firewall possui audit ring próprio e o Hook possui outro. Quando o datapath live for instanciado, isso pode consumir mais de 5 KiB por conjunto Engine+Hook.

Unificar em um evento de auditoria: parse status + direction + tuple IPv4 + verdict + rule id. Um ring de 32 ou 64 entradas basta inicialmente.

## PRIORIDADE ALTA — build e segurança

### 9. Build não é determinístico o suficiente

O Makefile usa `CXX ?= clang++`, `CC ?= clang`, `LD ?= ld.lld`, mas neste ambiente o build efetivo usou **g++ / cc / ld** por variáveis de ambiente já existentes.

Além disso, `CPP_SRCS := $(shell find kernel -name '*.cpp')` adiciona automaticamente qualquer `.cpp` colocado na árvore e a ordem pode depender do filesystem.

Para uma cadeia assinada isso é indesejável.

**Ação:** toolchain explícita/pinada ou versão registrada; lista explícita de fontes por perfil; no mínimo `$(sort ...)`. Qualification-only drivers não devem sequer ser compilados no SAFE.

### 10. Ativar garbage collection de seções

Adicionar `-ffunction-sections -fdata-sections` e `--gc-sections` passou nos testes ELF/integrity e reduziu o safe kernel imediatamente.

Resultado medido nesta auditoria:

- total: 71.882 B -> 68.794 B;
- text: 50.570 B -> 47.546 B.

### 11. Remover requests Limine não usados

Atualmente estão declarados mas não utilizados:

- firmware type request;
- EFI System Table request;
- EFI memory map request.

Os dois últimos eram necessários ao antigo bridge Runtime UEFI removido em Jó 1.0. Devem sair do kernel atual.

### 12. ACPI MCFG é apenas diagnóstico hoje

MCFG é parseado, armazenado e impresso, mas o PCI usa legacy config I/O. Enquanto ECAM não for usado, MCFG pode ficar atrás de um perfil diagnóstico ou ser adiado. MADT também é apenas informativo atualmente; pode permanecer se a próxima fase de interrupções/APIC estiver próxima.

### 13. Stack protector antes de NIC RX live

O kernel e o controlador EFI são compilados com `-fno-stack-protector`. Para bring-up isso simplificou o freestanding build, mas antes de aceitar frames externos vale implementar `__stack_chk_guard`/`__stack_chk_fail` com guard global compatível e habilitar proteção ao menos em código de parser/network/boot controller.

## PRIORIDADE MÉDIA — testes e organização

### 14. `safety.sh` depende demais de grep em fonte

Ele verifica strings/texto, não sempre comportamento/binário. Pode passar mesmo com perfil inconsistente.

Adicionar:

- teste de consistência de release profile;
- teste do ELF/EFI final para capacidades proibidas;
- build matrix SAFE/CURRENT/LKG/RECOVERY-LIVE;
- teste que proíbe números de geração/epoch hard-coded fora do profile central;
- teste de paridade controller ↔ journal ↔ Limine configs.

### 15. Unificar wrappers de testes

Há vários `*.sh + *_test.cpp` repetindo compilação com flags diferentes; alguns usam `c++`, outros `clang++`, alguns `-Werror`, outros não.

Centralizar flags e runner em Makefile/CMake simples ou `tests/run-host-test.sh`.

### 16. Separar release, qualification e archive

O pacote atual mistura:

- artifacts SAFE;
- artifacts RECOVERY-LIVE/QEMU;
- cópias duplicadas de LKG em `artifacts/` e `lkg-reference/`;
- 35 documentos históricos de fases anteriores.

Sugestão:

- `release/`: CURRENT safe + LKG + boot controller + configs + checksums;
- `qualification/`: recovery-live/QEMU/test kernels;
- `archive/`: docs históricos e snapshots antigos.

Isso não muda o kernel, mas reduz confusão operacional.

## Recursos que NÃO recomendo remover

- Limine como bootloader;
- GDT/IDT e tratamento real de exceções;
- page allocator e MMIO mapper próprio;
- GPT primário/backup;
- Recovery Anchor A/B (estático);
- Recovery Journal A/B (mutável);
- TPM anchor e Trusted Boot Controller;
- kernel `.text` integrity seal como defesa em profundidade;
- firewall default-deny e parser Ethernet/IPv4 mínimo;
- testes host-side — devem aumentar, não diminuir.

## Ordem recomendada antes de Muro 0.3

1. **Congelar novas features.**
2. Criar profile único e remover/hard-fail valores antigos.
3. Quarentenar `prepare-epoch-transition.sh`/commit antigos até serem regenerados pelo profile.
4. Remover Boot Control + rollback planner duplicado; usar Recovery Journal como estado operacional único.
5. Mover self-tests sintéticos para host/diagnostic profile.
6. Ativar section GC.
7. Gatear DMA32 e remover PMM probe-page.
8. Compactar PCI e catálogo de volumes.
9. Limpar requests Limine e scripts/docs/artifacts redundantes.
10. Só então iniciar **Muro 0.3 e1000 RX Sandbox**.

## Meta enxuta estimada

Sem mudar capacidades de produção, é realista mirar aproximadamente:

- safe kernel text: **~38–42 KiB** em vez de ~50,6 KiB;
- safe BSS: **~9–12 KiB** em vez de ~20,8 KiB;
- RAM reservada sem uso: recuperar **~2 MiB DMA32 + 4 KiB PMM probe** no SAFE;
- menos dois caminhos de decisão de rollback e uma família A/B inteira se Boot Control for removido.

Os valores finais devem ser medidos depois da refatoração; são metas, não resultados já implementados.
