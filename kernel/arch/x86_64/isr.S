.section .text
.extern peregrinus_exception_dispatch

.macro ISR_NOERR n
.global peregrinus_isr_\n
peregrinus_isr_\n:
    pushq $0
    pushq $\n
    jmp peregrinus_isr_common
.endm

.macro ISR_ERR n
.global peregrinus_isr_\n
peregrinus_isr_\n:
    pushq $\n
    jmp peregrinus_isr_common
.endm

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

peregrinus_isr_common:
    cld
    # stack here: vector, error, RIP, CS, RFLAGS, [RSP, SS if privilege change]
    movq 0(%rsp), %rdi
    movq 8(%rsp), %rsi
    movq 16(%rsp), %rdx
    movq 24(%rsp), %rcx
    movq 32(%rsp), %r8
    andq $-16, %rsp            # SysV ABI: 16-byte alignment at the call (handler never returns)
    call peregrinus_exception_dispatch
1:  cli
    hlt
    jmp 1b

.section .rodata
.global peregrinus_isr_table
.align 8
peregrinus_isr_table:
    .quad peregrinus_isr_0, peregrinus_isr_1, peregrinus_isr_2, peregrinus_isr_3
    .quad peregrinus_isr_4, peregrinus_isr_5, peregrinus_isr_6, peregrinus_isr_7
    .quad peregrinus_isr_8, peregrinus_isr_9, peregrinus_isr_10, peregrinus_isr_11
    .quad peregrinus_isr_12, peregrinus_isr_13, peregrinus_isr_14, peregrinus_isr_15
    .quad peregrinus_isr_16, peregrinus_isr_17, peregrinus_isr_18, peregrinus_isr_19
    .quad peregrinus_isr_20, peregrinus_isr_21, peregrinus_isr_22, peregrinus_isr_23
    .quad peregrinus_isr_24, peregrinus_isr_25, peregrinus_isr_26, peregrinus_isr_27
    .quad peregrinus_isr_28, peregrinus_isr_29, peregrinus_isr_30, peregrinus_isr_31

.section .note.GNU-stack,"",@progbits
