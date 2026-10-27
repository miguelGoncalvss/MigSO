#include <arch/i386/gdt.h>
#include <libc/string.h>

#define GDT_NUM_ENTRIES 6

static gdt_entry_t gdt_entries[GDT_NUM_ENTRIES];
static gdt_ptr_t   gdt_ptr;
static tss_entry_t tss_entry;

extern void gdt_flush(uint32_t gdt_ptr_addr);
extern void tss_flush(void);

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;

    gdt_entries[num].granularity |= (gran & 0xF0);
    gdt_entries[num].access      = access;
}

void tss_set_kernel_stack(uint32_t stack_top) {
    tss_entry.esp0 = stack_top;
}

static void write_tss(int num, uint16_t ss0, uint32_t esp0) {
    uint32_t base = (uint32_t)&tss_entry;
    uint32_t limit = sizeof(tss_entry_t) - 1;

    memset(&tss_entry, 0, sizeof(tss_entry_t));

    tss_entry.ss0  = ss0;
    tss_entry.esp0 = esp0;
    tss_entry.iomap_base = sizeof(tss_entry_t);

    // TSS descriptor: Access = 0x89 (Present=1, DPL=0, Type=Available 32-bit TSS)
    gdt_set_gate(num, base, limit, 0x89, 0x00);
}

void gdt_init(void) {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_NUM_ENTRIES) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    // 0: Null Descriptor
    gdt_set_gate(0, 0, 0, 0, 0);

    // 1: Kernel Code Segment (0x08) - Base 0, Limit 4GB, Access 0x9A, Gran 0xCF
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // 2: Kernel Data Segment (0x10) - Base 0, Limit 4GB, Access 0x92, Gran 0xCF
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // 3: User Code Segment (0x18 | 3 = 0x1B) - Base 0, Limit 4GB, Access 0xFA, Gran 0xCF
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    // 4: User Data Segment (0x20 | 3 = 0x23) - Base 0, Limit 4GB, Access 0xF2, Gran 0xCF
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    // 5: Task State Segment (TSS - 0x28)
    write_tss(5, 0x10, 0x1FFFF0);

    // Carrega a nova GDT e ativa o TSS no processador
    gdt_flush((uint32_t)&gdt_ptr);
    tss_flush();
}
