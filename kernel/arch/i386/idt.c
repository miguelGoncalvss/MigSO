#include <arch/i386/idt.h>

static struct idt_entry idt[256];
static struct idt_ptr idtp;

void idt_set_gate_flags(int n, unsigned int handler, unsigned char flags) {
    idt[n].offset_low  = (unsigned short)(handler & 0xFFFF);
    idt[n].selector    = 0x08; // Seletor de Codigo do Kernel (GDT 0x08)
    idt[n].zero        = 0;
    idt[n].type_attr   = flags;
    idt[n].offset_high = (unsigned short)((handler >> 16) & 0xFFFF);
}

void idt_set_gate(int n, unsigned int handler) {
    idt_set_gate_flags(n, handler, 0x8E); // Present, Ring 0, 32-bit Interrupt Gate
}

void idt_set_gate_user(int n, unsigned int handler) {
    // Present (0x80) | DPL=3 (0x60) | 32-bit Interrupt Gate (0x0E) = 0xEE
    idt_set_gate_flags(n, handler, 0xEE);
}

void idt_init(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (unsigned int)&idt;

    // Carrega a IDT no registrador IDTR da CPU
    __asm__ volatile ("lidtl (%0)" : : "r" (&idtp));
}
