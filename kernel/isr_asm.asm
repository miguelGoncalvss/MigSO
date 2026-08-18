[bits 32]
section .text

extern _isr_handler_c

%macro ISR_NOERRCODE 1
global _isr%1
global isr%1
_isr%1:
isr%1:
    push dword 0    ; Dummy error code
    push dword %1   ; Numero da interrupcao
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global _isr%1
global isr%1
_isr%1:
isr%1:
    push dword %1   ; Numero da interrupcao (CPU ja empurrou o erro)
    jmp isr_common_stub
%endmacro

; 0 a 31: Excecoes da CPU
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_ERRCODE   21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_ERRCODE   29
ISR_ERRCODE   30
ISR_NOERRCODE 31

isr_common_stub:
    pushal              ; Salva registradores gerais (eax, ecx, edx, ebx, esp, ebp, esi, edi)

    mov ax, ds
    push eax            ; Salva o descriptor de segmento de dados

    mov ax, 0x10        ; Carrega segmento de dados do kernel
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp            ; Passa ponteiro da struct registers_t para a funcao C
    call _isr_handler_c
    add esp, 4          ; Limpa argumento da pilha

    pop eax             ; Restaura segmento original
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popal               ; Restaura registradores gerais
    add esp, 8          ; Remove codigo de erro e numero da interrupcao
    iret