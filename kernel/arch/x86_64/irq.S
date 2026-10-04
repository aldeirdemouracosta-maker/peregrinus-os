.section .text
.extern peregrinus_irq_dispatch

# Hardware IRQ stubs for vectors 0x20-0x2F (8259 PIC). Save every caller-saved register,
# call the C dispatcher with the IRQ number, restore and return. The kernel is built with
# -mgeneral-regs-only and -mno-red-zone, so no SSE state or red zone needs protecting.
# Stack: CPU frame (40 B) + 9 pushes (72 B) = 112 B = 7*16, so the call is 16-byte aligned.
.macro IRQ n
.global peregrinus_irq_\n
peregrinus_irq_\n:
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    cld
    movl $\n, %edi
    call peregrinus_irq_dispatch
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rax
    iretq
.endm

# LAPIC spurious-interrupt vector (0xFF): no EOI, nothing to do.
.global peregrinus_lapic_spurious
peregrinus_lapic_spurious:
    iretq

IRQ 0
IRQ 1
IRQ 2
IRQ 3
IRQ 4
IRQ 5
IRQ 6
IRQ 7
IRQ 8
IRQ 9
IRQ 10
IRQ 11
IRQ 12
IRQ 13
IRQ 14
IRQ 15

.section .rodata
.global peregrinus_irq_table
.align 8
peregrinus_irq_table:
    .quad peregrinus_irq_0, peregrinus_irq_1, peregrinus_irq_2, peregrinus_irq_3
    .quad peregrinus_irq_4, peregrinus_irq_5, peregrinus_irq_6, peregrinus_irq_7
    .quad peregrinus_irq_8, peregrinus_irq_9, peregrinus_irq_10, peregrinus_irq_11
    .quad peregrinus_irq_12, peregrinus_irq_13, peregrinus_irq_14, peregrinus_irq_15

.section .note.GNU-stack,"",@progbits
