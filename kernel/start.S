.section .text
.global _start
.type _start, @function
.extern kmain
.extern peregrinus_stack_guard_init
_start:
    cli
    call peregrinus_stack_guard_init
    call kmain
1:
    hlt
    jmp 1b

# peregrinus_switch_stack(top, entry): continue on a new stack. Never returns.
.global peregrinus_switch_stack
.type peregrinus_switch_stack, @function
peregrinus_switch_stack:
    movq %rdi, %rsp
    xorl %ebp, %ebp
    call *%rsi
2:
    cli
    hlt
    jmp 2b

.section .note.GNU-stack,"",@progbits
