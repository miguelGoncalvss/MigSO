[bits 32]
section .text

global _gdt_flush
global gdt_flush
global _tss_flush
global tss_flush

_gdt_flush:
gdt_flush:
    mov eax, [esp + 4]  ; Endereco da estrutura gdt_ptr_t
    lgdt [eax]          ; Carrega a nova GDT na CPU

    ; Recarrega os seletores de segmento de dados para 0x10 (Kernel Data)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump para recarregar o registrador de codigo CS para 0x08 (Kernel Code)
    jmp 0x08:.reload_cs
.reload_cs:
    ret

_tss_flush:
tss_flush:
    ; Carrega o seletor da TSS (0x28) no registrador de tarefas TR da CPU
    mov ax, 0x28
    ltr ax
    ret
