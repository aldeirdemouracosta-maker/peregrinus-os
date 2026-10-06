# Peregrinus OS — instruções para o Claude

Kernel x86_64 freestanding (C++23, clang/ld.lld, boot via Limine) com controlador de boot UEFI em C.

## Invariantes (não negociáveis)

1. **Sem heap, sem libc, sem exceções/RTTI.** Estruturas são de tamanho fixo (ex.: Purgatório = 32 entradas, firewall/stateful/burst com limites constantes).
2. **Fail-closed sempre.** Entrada inválida, saturação ou falha de verificação negam/param — nunca permitem.
3. **O perfil SAFE é passivo.** Não assume NIC, não mapeia BAR Ethernet, não habilita bus mastering de rede, não reserva DMA32 para rede, não transmite. `tests/safe_binary_isolation.sh` garante isso.
4. **Perfis live são mutuamente exclusivos** (e1000-QEMU ≠ recovery/storage-write). Combinações inválidas falham em compile-time (`kernel/config/features.hpp`).
5. **Nenhuma alegação sem teste.** Docs e README só afirmam o que um teste prova; o resto vai como "NOT RUN"/não-objetivo.
6. **Boot path não altera NV do TPM.** Sem `NV_DefineSpace/Increment/Write/UndefineSpace` no kernel ou no controlador UEFI.
7. **Não adicionar subsistemas grandes** (antivírus, NAT, DHCP, IPv6, TCP, VPN…) antes de existir a base que os justifique.

## Comandos

- Build SAFE: `make`
- Regressão completa: `make check` (deve terminar com código 0)
- Qualificação em runtime (QEMU): `./scripts/fetch-limine.sh && ./scripts/qemu-qualify.sh`
- Fuzzing dos parsers: `FUZZ_SECONDS=60 ./tests/fuzz.sh`
- Artefatos reprodutíveis: `./scripts/make-artifacts.sh` (CI compara com `artifacts/SHA256SUMS`)
- Perfis: `make qemu-e1000-sandbox`, `make current-recovery-live`, `make trusted-boot-controller`, `make llm-local` (experimental; SSE só nele), `make ia-ponte` (SAFE + `pergunte` pela serial; lado Linux em `host/linux-ia/`)
- Limpeza: `make clean`

## Convenções

- Release profile autoritativo: `include/peregrinus/release_profile.h` (generation/epoch nunca hardcoded em scripts).
- Toda correção vem com teste host-side em `tests/` ligado ao `make check`.
- Ao alterar fontes, regenerar `SOURCE_SHA256SUMS` (inclui arquivos novos): `./scripts/update-source-sums.sh`
- Testes host usam `tests/host-cxx.sh` (mesmo compilador e flags, `-Werror`).
- Laços de espera de hardware: use `spin::until` (`kernel/runtime/spin.hpp`); nunca `while(spins--)` seguido de `if(!spins)`.
- Leia de memória mapeada por DMA/MMIO sempre via `volatile`; laços de espera devem ter orçamento que realmente expira.
