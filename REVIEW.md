# Regras de revisão

Bloqueante (🔴) se o PR:
- quebrar qualquer invariante do `CLAUDE.md` (heap, fail-open, SAFE com NIC/DMA live, perfis live combinados, NV do TPM no boot path);
- adicionar alegação em docs/README sem teste correspondente;
- alterar fonte sem atualizar `SOURCE_SHA256SUMS`;
- tiver laço de espera de hardware sem limite efetivo ou leitura de DMA/MMIO sem `volatile`;
- aumentar uma tabela fixa sem justificar o limite novo.

Não bloqueante: estilo, nomes, formatação compacta existente (o projeto usa C++ denso de propósito).
