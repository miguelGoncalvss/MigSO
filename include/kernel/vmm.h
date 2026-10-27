#ifndef KERNEL_VMM_H
#define KERNEL_VMM_H

#include <libc/stdint.h>
#include <libc/stddef.h>
#include <arch/i386/isr.h>

#define PAGE_SIZE               4096
#define PAGE_TABLE_ENTRIES      1024
#define PAGE_DIRECTORY_ENTRIES  1024

// Flags de Paginacao x86 de 2 Niveis
#define VMM_PAGE_PRESENT        0x001   // Bit 0: Pagina presente na memoria fisica
#define VMM_PAGE_WRITABLE       0x002   // Bit 1: 1 = Leitura/Escrita, 0 = Somente Leitura
#define VMM_PAGE_USER           0x004   // Bit 2: 1 = Ring 3 (User), 0 = Ring 0 (Supervisor)
#define VMM_PAGE_WRITETHROUGH   0x008   // Bit 3: Write-through caching
#define VMM_PAGE_CACHE_DISABLE  0x010   // Bit 4: Desabilita cache para a pagina
#define VMM_PAGE_ACCESSED       0x020   // Bit 5: Pagina acessada
#define VMM_PAGE_DIRTY          0x040   // Bit 6: Pagina escrita (dirty)
#define VMM_PAGE_SIZE_4MB       0x080   // Bit 7: Pagina de 4MB (quando PSE ativo)
#define VMM_PAGE_GLOBAL         0x100   // Bit 8: Pagina global (TLB persistente)

// Mascara de endereco base do frame de 4 KB (bits 12..31)
#define VMM_PAGE_FRAME_MASK     0xFFFFF000

typedef uint32_t page_entry_t;

typedef struct page_table {
    page_entry_t entries[PAGE_TABLE_ENTRIES];
} __attribute__((aligned(PAGE_SIZE))) page_table_t;

typedef struct page_directory {
    page_entry_t entries[PAGE_DIRECTORY_ENTRIES];
    // Ponteiros virtuais diretos para as tabelas gerenciadas pelo Kernel
    page_table_t* tables[PAGE_DIRECTORY_ENTRIES];
} __attribute__((aligned(PAGE_SIZE))) page_directory_t;

void vmm_init(void);
page_directory_t* vmm_get_kernel_directory(void);
page_directory_t* vmm_get_current_directory(void);
page_directory_t* vmm_create_page_directory(void);
void vmm_destroy_page_directory(page_directory_t* dir);
void vmm_switch_page_directory(page_directory_t* dir);

int  vmm_map_page(page_directory_t* dir, uint32_t virt_addr, uint32_t phys_addr, uint32_t flags);
void vmm_unmap_page(page_directory_t* dir, uint32_t virt_addr);
uint32_t vmm_get_phys_addr(page_directory_t* dir, uint32_t virt_addr);
int  vmm_is_mapped(page_directory_t* dir, uint32_t virt_addr);

void vmm_page_fault_handler(registers_t* regs);
void vmm_print_info(void);

#endif // KERNEL_VMM_H
