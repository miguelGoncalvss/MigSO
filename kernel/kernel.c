#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <kernel/kheap.h>
#include <fs/migfs.h>
#include <drivers/vga.h>
#include <arch/i386/idt.h>
#include <arch/i386/isr.h>
#include <arch/i386/pic.h>
#include <arch/i386/timer.h>
#include <drivers/keyboard.h>
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

    idt_init();
    isr_init();         // Registra tratadores de excecao 0-31
    pic_remap();
    timer_init(100);    // Inicializa o PIT em 100 Hz (1 tick = 10ms)
    keyboard_init();

    // Inicializa os gerenciadores de memoria física (PMM) e dinamica (KHeap)
    pmm_init(PMM_DEFAULT_RAM_SIZE);
    kheap_init(KHEAP_START_ADDRESS, KHEAP_INITIAL_SIZE);

    // Monta o sistema de arquivos RAMDisk (MIGFS) e carrega arquivos embutidos
    migfs_init();

    vga_puts("[OK] IDT, ISRs (0-31), PIC, PIT (100Hz) e Teclado ativos.\n");
    vga_puts("[OK] PMM (Frames 4KB) e KHeap (kmalloc) inicializados.\n");
    vga_puts("[OK] RAMDisk / MIGFS carregado com arquivos embutidos.\n");
    vga_puts("[OK] Shell interativo carregado.\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("========================================\n\n");

    shell_init();

    __asm__ volatile ("sti");

    while (1) {
        shell_update();
        __asm__ volatile ("hlt");
    }
}