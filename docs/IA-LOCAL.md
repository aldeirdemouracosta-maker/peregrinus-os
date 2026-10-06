# IA local no Peregrinus (perfil experimental `llm-local`)

O Peregrinus pode conversar com um **modelo de linguagem pequeno gravado no próprio pen drive**,
sem rede, sem nuvem e sem sistema operacional por baixo. É IA microscópica: modelos de dezenas a
centenas de MB, em uma CPU. Ela não se compara a assistentes como o Claude.

## Como funciona

1. O Limine carrega `model.bin` e `tokenizer.bin` do pen drive para a RAM como **módulos**, antes
   do kernel. O Peregrinus não precisa de driver USB nem de sistema de arquivos.
2. O kernel calcula o SHA-256 dos dois arquivos e passa pelo **Purgatório**: só carrega um par que
   esteja na lista de permissão `include/peregrinus/model_allowlist.h`. Um modelo adulterado ou
   desconhecido é recusado (fail-closed). Isso é testado no QEMU com um byte alterado.
3. Uma única área de memória contígua é reservada no carregamento (KV cache, buffers e
   tokenizer); não há alocação dinâmica depois.
4. O motor é um porte fiel do `run.c` do [llama2.c](https://github.com/karpathy/llama2.c)
   (MIT), o mesmo núcleo do [libclamma](https://github.com/warmcat/libclamma) (MIT), sem libc e
   com matemática própria (`kernel/llm/llm_math.cpp`, ≤ 1 ULP).
5. Só o perfil `llm-local` liga o SSE. Os perfis SAFE, e1000 e recovery continuam sem ponto
   flutuante e não contêm o motor (verificado em `tests/safe_binary_isolation.sh`).

## Garantia de correção

`tests/llm.sh` compila o `run.c` original (sem mudanças, em `tests/llm/run.c`) com a mesma
matemática e exige saída **idêntica byte a byte** à do motor do Peregrinus em modo
determinístico, com temperatura e com top-p. O cenário QEMU `llm` exige que a resposta do kernel
seja idêntica à do motor no PC. O teste usa um modelo determinístico gerado por
`scripts/make-test-model.py`; com pesos aleatórios, o texto não faz sentido, mas prova o motor.

## Usar com um modelo de verdade (stories15M)

No seu PC (Linux), com acesso à internet:

```sh
mkdir -p modelo && cd modelo
wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin -O model.bin
wget https://raw.githubusercontent.com/karpathy/llama2.c/master/tokenizer.bin
cd ..
./scripts/trust-model.py stories15M modelo/model.bin modelo/tokenizer.bin   # autoriza (SHA-256)
./scripts/fetch-limine.sh
MODEL_DIR=modelo ./scripts/make-iso.sh llm-local     # gera build-llm-local/...-llm-local.iso
sudo dd if=build-llm-local/peregrinus-purgatorio-0.1.1-llm-local.iso of=/dev/sdX bs=4M conv=fsync
```

O `dd` apaga tudo no pen drive; confira `/dev/sdX` com `lsblk`. Desative o Secure Boot. No
Peregrinus:

```
peregrinus> conversa Era uma vez um gato
peregrinus> conversa -g Era uma vez       (-g: determinístico, sempre a mesma resposta)
```

O stories15M (~60 MB) gera historinhas simples **em inglês**. Ele precisa de 256 MB de RAM ou
mais. A velocidade em hardware real ainda não foi medida (NOT RUN).

## Limites atuais

- **Formato:** só checkpoints llama2.c "legacy" float32 + `tokenizer.bin` estilo sentencepiece.
  Ainda não há suporte a int8 (`q80`).
- **SmolLM2 e Qwen:** usam outro tokenizador (BPE em bytes, estilo GPT-2) e, no caso do Qwen,
  bias nas projeções. Ainda não funcionam; precisariam de um tokenizador novo e de ajustes no motor.
- **Desempenho:** um núcleo, sem AVX; o laço não pode ser interrompido no meio (no máximo 128
  posições por pergunta).
- **Sem memória de conversa:** cada `conversa` é uma completação independente.
- **Não testado:** em hardware real.

Para um modelo bem maior, na GPU de um Linux ao lado (ex.: Tesla P100), veja `docs/IA-GPU.md`
(perfil `ia-ponte`, comando `pergunte`).
