; ============================================================
; migOS - Bootloader Definitivo (512 bytes)
; ============================================================

BITS 16
ORG 0x7C00

KERNEL_LOAD_SEGMENT equ 0x1000
KERNEL_LOAD_OFFSET  equ 0x0000
SECTORS_TO_LOAD     equ 15

start:
    cli

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [boot_drive], dl
    sti

    mov si, boot_message
    call print_string

    ; Carrega o Kernel do disco para 0x1000:0x0000 (0x10000 linear)
    call disk_load

    ; Desabilita interrupções da BIOS antes de mudar de modo
    cli
    xor ax, ax
    mov ds, ax

    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:protected_mode_start

; ============================================================
; ROTINAS 16-BIT
; ============================================================

disk_load:
    mov ax, KERNEL_LOAD_SEGMENT
    mov es, ax
    mov bx, KERNEL_LOAD_OFFSET

    mov ah, 0x02
    mov al, SECTORS_TO_LOAD
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]

    int 0x13
    jc disk_error
    ret

disk_error:
    xor ax, ax
    mov ds, ax
    mov si, disk_error_message
    call print_string
.error_halt:
    cli
    hlt
    jmp .error_halt

print_string:
.next_char:
    lodsb
    cmp al, 0
    je .done
    mov ah, 0x0E
    int 0x10
    jmp .next_char
.done:
    ret

; ============================================================
; DADOS & GDT
; ============================================================

boot_message:       db 13, 10, "migOS: Carregando Kernel...", 13, 10, 0
disk_error_message: db 13, 10, "Erro ao ler Kernel do disco!", 13, 10, 0
boot_drive:         db 0

gdt_start:
gdt_null:
    dq 0x0000000000000000

gdt_code:
    dw 0xFFFF, 0x0000
    db 0x00, 0x9A, 0xCF, 0x00

gdt_data:
    dw 0xFFFF, 0x0000
    db 0x00, 0x92, 0xCF, 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; ============================================================
; PROTECTED MODE (32-BIT)
; ============================================================

BITS 32
protected_mode_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x90000
    mov esp, 0x90000

    ; Salta para o ponto de entrada do kernel compilado em C
    jmp 0x10000

; ============================================================
; ASSINATURA DE BOOT (EXATOS 512 BYTES)
; ============================================================

times 510 - ($ - $$) db 0
dw 0xAA55