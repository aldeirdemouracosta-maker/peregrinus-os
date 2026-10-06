# HolyC no Peregrinus (modelo híbrido)

O Peregrinus usa um **modelo híbrido**: o kernel continua em C++ (seguro, testado e sem heap), e o
shell aceita um **subconjunto de HolyC**, a linguagem do TempleOS, executado por um
interpretador com limites fixos. Você digita código e ele roda na hora, como no TempleOS. Um
script ruim, no máximo, é interrompido pelo limite; ele não derruba o sistema.

Diferenças deliberadas em relação ao TempleOS:
- O TempleOS compila HolyC com JIT e o executa no nível do kernel, com acesso total à memória.
  Aqui o código é **interpretado** e não tem ponteiros, portas de E/S nem acesso ao estado do
  kernel.
- Implementação própria (`kernel/shell/holyc.cpp`). O projeto
  [Jamesbarford/holyc-lang](https://github.com/Jamesbarford/holyc-lang) (BSD-2) serviu de
  referência da linguagem; nenhum código foi copiado.

## Usar no shell

```
peregrinus> hc "Olá, %d\n", 6*7;
Olá, 42
peregrinus> hc
HolyC: digite o programa; uma linha 'fim' executa.
hc> I64 i;
hc> for (i = 1; i <= 15; i++) {
hc>   if (i % 15 == 0) "FizzBuzz\n";
hc>   else if (i % 3 == 0) "Fizz\n";
hc>   else if (i % 5 == 0) "Buzz\n";
hc>   else "%d\n", i;
hc> }
hc> fim
```

## O que o subconjunto suporta

| Recurso | Exemplo |
|---|---|
| Variáveis inteiras de 64 bits | `I64 a = 1, b;` |
| Atribuição | `a = 3; a += 2; a -= 1; a *= 4; a /= 2; a %= 5; a++; a--;` |
| Condição | `if (a > 3) ... else ...` |
| Laços | `while (cond) ...`, `for (I64 i = 0; i < 10; i++) ...`, `break;`, `continue;` |
| Blocos e comentários | `{ ... }`, `// linha`, `/* bloco */` |
| Impressão (estilo HolyC) | `"texto\n";`, `"x=%d\n", x;`, `Print("%x", 255);` |
| Formatos | `%d` `%i` `%x` `%X` `%c` `%%` |
| Operadores | `|| && == != < > <= >= + - * / %`, `-x`, `!x`, parênteses |
| Literais | `42`, `0x2A`, `'A'` |

Aritmética de 64 bits com complemento de dois (o estouro "dá a volta"). Texto acentuado digitado
no shell (Latin-1) sai em UTF-8.

## Limites (todos fail-closed: o programa para com erro e número da linha)

| Limite | Valor |
|---|---|
| Tamanho do programa | 2048 bytes |
| Variáveis | 32 |
| Nome de variável | 15 caracteres |
| Passos de execução | 200.000 (para laços infinitos) |
| Aninhamento (blocos e expressões) | 48 |
| Saída por execução | 8192 bytes |
| Argumentos de formato | 8 |

Erros também param o programa: divisão por zero, variável não declarada, formato inválido,
argumentos faltando ou sobrando, número grande demais, string ou comentário sem fechamento.

## Ainda não suportado

Funções, tipos além de `I64` (U8, F64, classes), ponteiros, arrays, strings como valores,
`switch`, `do/while`, operadores de bits. Funções e arrays limitados seriam o próximo passo
natural, sempre com limites fixos.

## Testes

- `tests/holyc.sh`: semântica, cada limite e cada erro.
- `tests/fuzz.sh`: o interpretador entra no fuzzing com libFuzzer + ASan/UBSan.
- Cenário QEMU `shell`: linha única, bloco de várias linhas e laço infinito barrado pelo
  limite, digitados pela serial.
