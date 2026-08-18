#include "isr.h"
#include "idt.h"
#include "vga.h"
#include "io.h"

static const char* exception_messages[32] = {
    "Division By Zero Exception",
    "Debug Exception",
    "Non Maskable Interrupt Exception",
    "Breakpoint Exception",
    "Into Detected Overflow Exception",
    "Out of Bounds Exception",
    "Invalid Opcode Exception",
    "No Coprocessor Exception",
    "Double Fault Exception",
    "Coprocessor Segment Overrun Exception",
    "Bad TSS Exception",
    "Segment Not Present Exception",
    "Stack Fault Exception",
    "General Protection Fault",
    "Page Fault Exception",
    "Unknown Interrupt Exception",
    "Coprocessor Fault Exception",
    "Alignment Check Exception",
    "Machine Check Exception",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved"
};

extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

static void print_hex(unsigned int n) {
    char hex_chars[] = "0123456789ABCDEF";
    vga_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        vga_putc(hex_chars[(n >> i) & 0xF]);
    }
}

void isr_init(void) {
    idt_set_gate(0,  (unsigned int)isr0);
    idt_set_gate(1,  (unsigned int)isr1);
    idt_set_gate(2,  (unsigned int)isr2);
    idt_set_gate(3,  (unsigned int)isr3);
    idt_set_gate(4,  (unsigned int)isr4);
    idt_set_gate(5,  (unsigned int)isr5);
    idt_set_gate(6,  (unsigned int)isr6);
    idt_set_gate(7,  (unsigned int)isr7);
    idt_set_gate(8,  (unsigned int)isr8);
    idt_set_gate(9,  (unsigned int)isr9);
    idt_set_gate(10, (unsigned int)isr10);
    idt_set_gate(11, (unsigned int)isr11);
    idt_set_gate(12, (unsigned int)isr12);
    idt_set_gate(13, (unsigned int)isr13);
    idt_set_gate(14, (unsigned int)isr14);
    idt_set_gate(15, (unsigned int)isr15);
    idt_set_gate(16, (unsigned int)isr16);
    idt_set_gate(17, (unsigned int)isr17);
    idt_set_gate(18, (unsigned int)isr18);
    idt_set_gate(19, (unsigned int)isr19);
    idt_set_gate(20, (unsigned int)isr20);
    idt_set_gate(21, (unsigned int)isr21);
    idt_set_gate(22, (unsigned int)isr22);
    idt_set_gate(23, (unsigned int)isr23);
    idt_set_gate(24, (unsigned int)isr24);
    idt_set_gate(25, (unsigned int)isr25);
    idt_set_gate(26, (unsigned int)isr26);
    idt_set_gate(27, (unsigned int)isr27);
    idt_set_gate(28, (unsigned int)isr28);
    idt_set_gate(29, (unsigned int)isr29);
    idt_set_gate(30, (unsigned int)isr30);
    idt_set_gate(31, (unsigned int)isr31);
}

void isr_handler_c(registers_t* regs) {
    __asm__ volatile ("cli");

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    vga_clear();

    vga_puts("================================================================================\n");
    vga_puts("                            *** KERNEL PANIC ***                                \n");
    vga_puts("================================================================================\n\n");

    vga_puts(" Excecao da CPU: ");
    if (regs->int_no < 32) {
        vga_puts(exception_messages[regs->int_no]);
    } else {
        vga_puts("Desconhecida");
    }
    
    vga_puts("\n Codigo de Erro: ");
    print_hex(regs->err_code);
    vga_puts("  |  Vetor IDT: ");
    print_hex(regs->int_no);
    vga_puts("\n\n Dump dos Registradores:\n");

    vga_puts(" EIP: "); print_hex(regs->eip);
    vga_puts("  CS:  "); print_hex(regs->cs);
    vga_puts("  EFLAGS: "); print_hex(regs->eflags);
    vga_puts("\n EAX: "); print_hex(regs->eax);
    vga_puts("  EBX: "); print_hex(regs->ebx);
    vga_puts("  ECX:    "); print_hex(regs->ecx);
    vga_puts("  EDX: "); print_hex(regs->edx);
    vga_puts("\n ESP: "); print_hex(regs->esp);
    vga_puts("  EBP: "); print_hex(regs->ebp);
    vga_puts("  ESI:    "); print_hex(regs->esi);
    vga_puts("  EDI: "); print_hex(regs->edi);

    vga_puts("\n\n O kernel foi congelado para proteger o sistema.\n");
    vga_puts(" Reinicie a maquina virtual.\n");

    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}