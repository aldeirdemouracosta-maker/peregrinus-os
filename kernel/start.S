.section .text
.global _start
.type _start, @function
.extern kmain
_start:
    cli
    call kmain
1:
    hlt
    jmp 1b
