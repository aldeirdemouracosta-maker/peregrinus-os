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
- Perfis: `make qemu-e1000-sandbox`, `make current-recovery-live`, `make trusted-boot-controller`
- Boot real no QEMU/OVMF: `make qemu-boot-test` (precisa de qemu-system-x86, ovmf, xorriso, nasm, mtools)
- Limpeza: `make clean`

## Convenções

- Release profile autoritativo: `include/peregrinus/release_profile.h` (generation/epoch nunca hardcoded em scripts).
- Toda correção vem com teste host-side em `tests/` ligado ao `make check`.
- Binários e checksums não vão para o git: o workflow `release.yml` (tag `v*`) compila, qualifica e publica binários, `SHA256SUMS` e `SOURCE_SHA256SUMS` nos Releases.
- Leia de memória mapeada por DMA/MMIO sempre via `volatile`; laços de espera devem ter orçamento que realmente expira.
