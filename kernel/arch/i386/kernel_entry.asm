[bits 32]
section .text

extern _kernel_main
extern __bss_start
extern __bss_end

global __start
global _start

__start:
_start:
    mov esp, 0x1FFFF0
    mov ebp, 0x1FFFF0

    ; Limpa toda a secao BSS com zeros
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, __bss_start
    xor eax, eax
    cld
    rep stosb

    call _kernel_main

.hang:
    cli
    hlt
    jmp .hang
