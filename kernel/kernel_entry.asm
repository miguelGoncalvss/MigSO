[bits 32]
section .text

extern _kernel_main

global __start
global _start

__start:
_start:
    call _kernel_main

.hang:
    cli
    hlt
    jmp .hang