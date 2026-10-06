# Rodar o Peregrinus OS numa máquina virtual

O Peregrinus é um kernel: ele precisa controlar o hardware sozinho, então não roda **ao lado**
do Windows/Linux no mesmo computador. Para usá-lo sem reiniciar o PC, rode-o numa máquina
virtual, numa janela da sua área de trabalho.

## O que esperar

- O log do boot aparece **na tela** (console de texto no framebuffer) e também na **porta serial (COM1)**.
  A serial continua útil para salvar o log num arquivo.
- Depois do diagnóstico aparece o prompt `peregrinus>`. Digite `ajuda` para ver os comandos (teclado PS/2 ou pela serial).
  O teclado padrão é ABNT2 (Brasil); `teclado us` troca para o americano.
- Use o perfil **SAFE** (`peregrinus-...-safe.iso`). O perfil **e1000** só aceita QEMU/KVM e se recusa
  a rodar em outros hipervisores, de propósito.
- Memória: 256 MB sobram. Desative o Secure Boot (o Limine não é assinado).

## Gerar a ISO

```sh
./scripts/fetch-limine.sh
./scripts/make-iso.sh safe      # imprime o caminho: build/peregrinus-<tag>-safe.iso
```

## QEMU — testado

Testado por `scripts/qemu-qualify.sh` (BIOS e UEFI) e manualmente como pen drive USB.

```sh
# BIOS
qemu-system-x86_64 -m 256M -cdrom build/peregrinus-*-safe.iso -serial stdio
# UEFI (Ubuntu/Debian: pacote ovmf)
qemu-system-x86_64 -m 256M -bios /usr/share/ovmf/OVMF.fd -cdrom build/peregrinus-*-safe.iso -serial stdio
```

O log aparece no terminal. Atalho equivalente: `./scripts/run-qemu.sh safe uefi`.
No Windows, instale o QEMU oficial e use o mesmo comando no PowerShell.

## VirtualBox — NOT RUN

Usa o mesmo boot BIOS/UEFI que o QEMU, mas **ainda não foi testado**.

1. **Nova VM**: Tipo *Other*, Versão *Other/Unknown (64-bit)*, 256 MB de RAM, sem disco rígido.
2. **Armazenamento**: no controlador IDE, escolha a ISO no drive óptico.
3. **Portas Seriais → Porta 1**: habilitar, COM1, modo *Arquivo bruto*, caminho para um `peregrinus.log`.
4. (Opcional) **Sistema → Habilitar EFI** para testar o caminho UEFI.
5. Inicie a VM e abra o `peregrinus.log`. A última linha esperada começa com `Boot complete:`.

## Hyper-V (Windows Pro/Enterprise) — NOT RUN

1. Nova VM de **Geração 1**, 256 MB de RAM, sem disco. A Geração 2 não tem teclado PS/2
   (usa um teclado sintético), então o shell do Peregrinus não receberia o teclado nela.
   Isso é dedução a partir do hardware que cada geração emula, não um teste.
2. **Unidade de DVD**: a ISO.
3. A saída serial do Hyper-V vai para um *named pipe*. No PowerShell (admin):
   `Set-VMComPort -VMName Peregrinus -Number 1 -Path \\.\pipe\peregrinus`,
   e leia o pipe com um cliente de pipe (por exemplo PuTTY, conexão *Serial*, linha `\\.\pipe\peregrinus`).

## Se não aparecer nada no log

- Confira se a serial está ligada à **COM1**.
- `KERNEL PANIC`/`CPU EXCEPTION` no log: anote as linhas seguintes (Reason/Vector/RIP) e abra uma issue.
- Funcionou no VirtualBox ou Hyper-V? Registre o resultado para trocar o NOT RUN acima por um teste.
