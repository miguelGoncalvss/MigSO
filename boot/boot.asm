; ============================================================
; migOS - Bootloader Definitivo com Leitura LBA em Chunks
; ============================================================

BITS 16
ORG 0x7C00

KERNEL_START_SEGMENT equ 0x1000
CHUNK_SECTORS        equ 64      ; 64 setores = 32 KB por chamada (seguro para todas as BIOS)
TOTAL_CHUNKS         equ 16      ; 16 * 64 = 1024 setores = 512 KB

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
    call disk_load_chunks

    ; Habilita a linha de endereco A20 (Fast A20)
    in al, 0x92
    or al, 2
    out 0x92, al

    ; Desabilita interrupcoes da BIOS antes de mudar para Protected Mode
    cli
    xor ax, ax
    mov ds, ax

    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:protected_mode_start

; ============================================================
; ROTINAS 16-BIT DE LEITURA DE DISCO EM CHUNKS (LBA)
; ============================================================

disk_load_chunks:
    ; Verifica se extenções LBA (INT 13h, AH=0x41) estão disponíveis
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc disk_error
    cmp bx, 0xAA55
    jne disk_error

    mov cx, TOTAL_CHUNKS

.chunk_loop:
    push cx

    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    ; Imprime um ponto '.' para cada bloco carregado (progresso visual)
    mov al, '.'
    mov ah, 0x0E
    int 0x10

    ; Atualiza o DAP para o proximo bloco de 64 setores (32 KB)
    add word [dap_lba_lo], CHUNK_SECTORS
    adc word [dap_lba_hi], 0
    add word [dap_segment], (CHUNK_SECTORS * 512) / 16  ; += 0x0800

    pop cx
    loop .chunk_loop

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
; DADOS, DAP & GDT
; ============================================================

boot_message:       db 13, 10, "migOS: Carregando", 0
disk_error_message: db 13, 10, "Erro ao ler disco!", 13, 10, 0
boot_drive:         db 0

align 4
dap:
    db 0x10                 ; Tamanho do pacote DAP (16 bytes)
    db 0                    ; Reservado (0)
    dw CHUNK_SECTORS        ; 64 setores por leitura (32 KB)
    dw 0x0000               ; Offset no segmento
dap_segment:
    dw KERNEL_START_SEGMENT ; Inicia em 0x1000
dap_lba_lo:
    dw 1                    ; LBA inicial = 1
dap_lba_hi:
    dw 0
    dd 0                    ; LBA 64-bit high dword

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

    mov ebp, 0x1FFFF0
    mov esp, 0x1FFFF0

    ; Salta para o ponto de entrada do kernel compilado em C
    jmp 0x10000

; ============================================================
; ASSINATURA DE BOOT (EXATOS 512 BYTES)
; ============================================================

times 510 - ($ - $$) db 0
dw 0xAA55