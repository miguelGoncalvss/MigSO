#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <kernel/kheap.h>
#include <kernel/vmm.h>
#include <kernel/task.h>
#include <kernel/syscall.h>
#include <fs/migfs.h>
#include <drivers/vga.h>
#include <arch/i386/gdt.h>
#include <arch/i386/idt.h>
#include <arch/i386/isr.h>
#include <arch/i386/pic.h>
#include <arch/i386/timer.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/ata.h>
#include <drivers/rtc.h>
#include <drivers/sound.h>
#include <shell/shell.h>

void kernel_main(void)
{
    vga_init();

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("========================================\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("migOS Kernel ");
    vga_puts(MIVOS_VERSION);
    vga_puts(" pronto!\n");
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    vga_puts("[OK] Video VGA Driver carregado.\n");

    // 1. Arquitetura Basica x86: GDT, TSS, IDT, ISRs e Syscalls
    gdt_init();         // Configura GDT de 6 entradas (Kernel Ring 0, User Ring 3 e TSS)
    idt_init();         // Inicializa a tabela IDT (256 vetores)
    isr_init();         // Registra tratadores de excecao de CPU (0-31)
    syscall_init();     // Registra a interface de Syscalls (Vetor 128 / 0x80 com DPL=3)
    pic_remap();        // Remapeia o PIC 8259A (IRQs 0-15 -> Vetores 32-47)
    timer_init(100);    // Inicializa o PIT 8254 em 100 Hz (1 tick = 10ms - Scheduler Preemptivo)
    rtc_init();         // Inicializa o Real-Time Clock (CMOS / RTC)
    sound_init();       // Inicializa o Driver de Audio PC Speaker (PIT Canal 2 / Porta 0x61)
    keyboard_init();    // Inicializa Teclado PS/2 (IRQ 1)
    mouse_init();       // Inicializa Mouse PS/2 (IRQ 12)
    ata_init();         // Inicializa o disco ATA/IDE primario (PIO Mode)

    // 2. Gerenciamento de Memoria: PMM (Frames 4KB), KHeap e VMM (Paginacao de 2 Niveis)
    pmm_init(PMM_DEFAULT_RAM_SIZE);
    kheap_init(KHEAP_START_ADDRESS, KHEAP_INITIAL_SIZE);
    vmm_init();         // Ativa paginacao x86 no CR0.PG e CR3 (Isolamento de Memoria Ring 0/3)

    // 3. Subssistema de Processos e Multitarefa Preemptiva (Round-Robin acionado por IRQ0)
    task_init();        // Inicializa o Scheduler, PCB e Processo 0 (Shell/Kernel)

    // 4. Sistema de Arquivos RAMDisk Persistente (MIGFS)
    migfs_init();

    vga_puts("[OK] GDT (Ring 0/3), TSS, IDT, Syscalls (int 0x80), PIC, PIT (100Hz).\n");
    vga_puts("[OK] PMM (4KB Frames), KHeap (8MB), VMM (2-Level Paging CR3/CR0.PG).\n");
    vga_puts("[OK] Preemptive Multitasking Scheduler (Round-Robin PCB) ativo.\n");
    vga_puts("[OK] RAMDisk / MIGFS e Subssistema DOOM Bare-Metal prontos.\n");
    vga_puts("[OK] Shell interativo carregado.\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("========================================\n\n");

    shell_init();

    // Habilita as interrupcoes de hardware no processador x86
    __asm__ volatile ("sti");

    // Toca o Mac OS Classic Startup Chime na inicializacao
    sound_play_sfx(SFX_STARTUP);

    while (1) {
        shell_update();
        __asm__ volatile ("hlt");
    }
}