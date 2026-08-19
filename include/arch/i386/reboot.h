#ifndef ARCH_I386_REBOOT_H
#define ARCH_I386_REBOOT_H

#include <arch/i386/io.h>

static inline void reboot_system(void) {
    // 1. Reset via Fast PCI reset (porta 0xCF9)
    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);

    // 2. Reset via 8042 Keyboard Controller (pulso na linha de reset da CPU)
    outb(0x64, 0xFE);

    // 3. Triple Fault forçado (carrega IDT com limite 0 e dispara interrupcao)
    __asm__ volatile (
        "cli\n"
        "pushl $0\n"
        "pushl $0\n"
        "lidt (%esp)\n"
        "int $3\n"
    );

    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

#endif // ARCH_I386_REBOOT_H
