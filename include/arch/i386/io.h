#ifndef ARCH_I386_IO_H
#define ARCH_I386_IO_H

// Escreve 1 byte em uma porta de I/O
static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Lê 1 byte de uma porta de I/O
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Escreve 2 bytes (word) em uma porta de I/O
static inline void outw(unsigned short port, unsigned short val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

// Lê 2 bytes (word) de uma porta de I/O
static inline unsigned short inw(unsigned short port) {
    unsigned short ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Escreve 4 bytes (dword) em uma porta de I/O
static inline void outl(unsigned short port, unsigned int val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

// Lê 4 bytes (dword) de uma porta de I/O
static inline unsigned int inl(unsigned short port) {
    unsigned int ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Espera uma operação de I/O (pequeno delay de sincronização)
static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif // ARCH_I386_IO_H
