#include "vga.h"
#include "idt.h"
#include "isr.h"
#include "pic.h"
#include "keyboard.h"
#include "shell.h"

void kernel_main(void)
{
    vga_init();

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("========================================\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("migOS Kernel v0.5 pronto!\n");
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    vga_puts("[OK] Video VGA Driver carregado.\n");

    idt_init();
    isr_init();         // Registra tratadores de exceção 0-31
    pic_remap();
    keyboard_init();

    vga_puts("[OK] IDT, ISRs (0-31), PIC e Teclado ativos.\n");
    vga_puts("[OK] Shell interativo carregado.\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("========================================\n\n");

    shell_init();

    __asm__ volatile ("sti");

    while (1) {
        __asm__ volatile ("hlt");
    }
}