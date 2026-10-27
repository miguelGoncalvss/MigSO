#ifndef ARCH_I386_GDT_H
#define ARCH_I386_GDT_H

#include <libc/stdint.h>

#define GDT_KERNEL_CODE_SEGMENT 0x08
#define GDT_KERNEL_DATA_SEGMENT 0x10
#define GDT_USER_CODE_SEGMENT   (0x18 | 3) // 0x1B
#define GDT_USER_DATA_SEGMENT   (0x20 | 3) // 0x23
#define GDT_TSS_SEGMENT         0x28

// Estrutura de uma entrada na GDT (8 bytes)
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));
typedef struct gdt_entry gdt_entry_t;

// Ponteiro do registrador GDTR (6 bytes)
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));
typedef struct gdt_ptr gdt_ptr_t;

// Estrutura do Task State Segment (TSS - 104 bytes)
struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;       // Pilha de kernel (Ring 0) carregada nas interrupcoes/syscalls
    uint32_t ss0;        // Segmento de pilha de kernel (0x10)
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));
typedef struct tss_entry tss_entry_t;

void gdt_init(void);
void tss_set_kernel_stack(uint32_t stack_top);

#endif // ARCH_I386_GDT_H
