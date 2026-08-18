#include "pic.h"
#include "io.h"

void pic_remap(void) {
    // Salva máscaras
    unsigned char a1 = inb(PIC1_DATA);
    unsigned char a2 = inb(PIC2_DATA);

    // Inicialização em cascata (ICW1)
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);

    // ICW2: Vetor base (Master = 32, Slave = 40)
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);

    // ICW3: Cascata
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);

    // ICW4: Modo 8086
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    // Restaura máscaras (desmascara apenas IRQ1 = Teclado)
    outb(PIC1_DATA, 0xFD); // 11111101b (habilita apenas bit 1: Teclado)
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(unsigned char irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}