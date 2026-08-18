#include <arch/i386/pic.h>
#include <arch/i386/io.h>

void pic_remap(void) {
    // Salva mascaras
    unsigned char a1 = inb(PIC1_DATA);
    unsigned char a2 = inb(PIC2_DATA);

    // Inicializacao em cascata (ICW1)
    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();

    // ICW2: Vetor base (Master = 32, Slave = 40)
    outb(PIC1_DATA, 0x20);
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();

    // ICW3: Cascata
    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    // ICW4: Modo 8086
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    // Restaura mascaras (desmascara IRQ0 = Timer e IRQ1 = Teclado)
    (void)a1;
    (void)a2;
    outb(PIC1_DATA, 0xFC); // 11111100b (habilita bit 0: Timer, bit 1: Teclado)
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(unsigned char irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_unmask_irq(unsigned char irq) {
    unsigned short port;
    unsigned char val;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    val = inb(port) & ~(1 << irq);
    outb(port, val);
}

void pic_mask_irq(unsigned char irq) {
    unsigned short port;
    unsigned char val;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    val = inb(port) | (1 << irq);
    outb(port, val);
}
