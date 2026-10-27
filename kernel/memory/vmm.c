#include <kernel/vmm.h>
#include <kernel/pmm.h>
#include <kernel/kheap.h>
#include <kernel/task.h>
#include <arch/i386/idt.h>
#include <drivers/vga.h>
#include <drivers/bga.h>
#include <libc/string.h>
#include <libc/stdlib.h>

#define KERNEL_IDENTITY_TABLES  16      // 16 tabelas * 1024 entradas * 4KB = 64 MB mapeados em identidade (RAM do QEMU)

static page_directory_t kernel_directory __attribute__((aligned(PAGE_SIZE)));
static page_table_t     kernel_tables[KERNEL_IDENTITY_TABLES] __attribute__((aligned(PAGE_SIZE)));
static page_directory_t* current_directory = &kernel_directory;

static inline void vmm_invlpg(uint32_t addr) {
    __asm__ volatile ("invlpg (%0)" : : "r"(addr) : "memory");
}

void vmm_init(void) {
    memset(&kernel_directory, 0, sizeof(page_directory_t));

    // Mapeamento identidade dos primeiros 64 MB (Kernel, Stack, Heap, VGA, PMM)
    for (uint32_t t = 0; t < KERNEL_IDENTITY_TABLES; t++) {
        memset(&kernel_tables[t], 0, sizeof(page_table_t));

        for (uint32_t p = 0; p < PAGE_TABLE_ENTRIES; p++) {
            uint32_t phys_addr = (t * PAGE_TABLE_ENTRIES + p) * PAGE_SIZE;
            // Supervisor, Read/Write, Present
            kernel_tables[t].entries[p] = phys_addr | VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE;
        }

        // Registra a tabela no Page Directory do Kernel
        kernel_directory.entries[t] = ((uint32_t)&kernel_tables[t]) | VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE;
        kernel_directory.tables[t]  = &kernel_tables[t];
    }

    current_directory = &kernel_directory;

    // Mapeia o Framebuffer Linear do BGA (LFB) para acesso direto com paginacao ativa
    uint32_t lfb_addr = bga_get_lfb_address();
    if (lfb_addr != 0) {
        // Mapeia 4 MB (1024 paginas de 4KB) para cobrir o Framebuffer BGA (1.2 MB para 640x480x4)
        for (uint32_t i = 0; i < 1024; i++) {
            uint32_t page_addr = lfb_addr + (i * PAGE_SIZE);
            vmm_map_page(&kernel_directory, page_addr, page_addr, VMM_PAGE_WRITABLE | VMM_PAGE_PRESENT);
        }
    }

    // Carrega o CR3 com o endereco fisico do Page Directory e ativa a paginacao no CR0
    __asm__ volatile (
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or $0x80000001, %%eax\n"
        "mov %%eax, %%cr0\n"
        : : "r"(&kernel_directory) : "eax", "memory"
    );
}

page_directory_t* vmm_get_kernel_directory(void) {
    return &kernel_directory;
}

page_directory_t* vmm_get_current_directory(void) {
    return current_directory;
}

page_directory_t* vmm_create_page_directory(void) {
    page_directory_t* dir = (page_directory_t*)pmm_alloc_block();
    if (!dir) {
        dir = (page_directory_t*)kmalloc(sizeof(page_directory_t));
    }
    if (!dir) return NULL;

    memset(dir, 0, sizeof(page_directory_t));

    // Clona as entradas do Kernel e dispositivos mapeados (como o LFB do BGA)
    for (int i = 0; i < PAGE_DIRECTORY_ENTRIES; i++) {
        if (kernel_directory.tables[i] != NULL) {
            dir->entries[i] = kernel_directory.entries[i];
            dir->tables[i]  = kernel_directory.tables[i];
        }
    }

    return dir;
}

void vmm_destroy_page_directory(page_directory_t* dir) {
    if (!dir || dir == &kernel_directory) return;

    // Desaloca apenas tabelas criadas no espaco de usuario (que nao pertencem ao Kernel)
    for (int i = 0; i < PAGE_DIRECTORY_ENTRIES; i++) {
        if (dir->tables[i] != NULL && dir->tables[i] != kernel_directory.tables[i]) {
            pmm_free_block(dir->tables[i]);
            dir->tables[i] = NULL;
            dir->entries[i] = 0;
        }
    }

    pmm_free_block(dir);
}

void vmm_switch_page_directory(page_directory_t* dir) {
    if (!dir) dir = &kernel_directory;
    current_directory = dir;

    __asm__ volatile (
        "mov %0, %%cr3\n"
        : : "r"(dir) : "memory"
    );
}

int vmm_map_page(page_directory_t* dir, uint32_t virt_addr, uint32_t phys_addr, uint32_t flags) {
    if (!dir) dir = current_directory;

    uint32_t pd_index = virt_addr >> 22;
    uint32_t pt_index = (virt_addr >> 12) & 0x03FF;

    // Se a tabela de paginas ainda nao existir neste diretorio, cria uma nova
    if (dir->tables[pd_index] == NULL) {
        page_table_t* new_pt = (page_table_t*)pmm_alloc_block();
        if (!new_pt) {
            new_pt = (page_table_t*)kmalloc(sizeof(page_table_t));
        }
        if (!new_pt) return -1;

        memset(new_pt, 0, sizeof(page_table_t));

        // Permite flags de usuario se solicitado
        uint32_t pd_flags = VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | (flags & VMM_PAGE_USER);
        dir->entries[pd_index] = ((uint32_t)new_pt) | pd_flags;
        dir->tables[pd_index]  = new_pt;
    } else {
        // Se a pagina mapeada for de usuario, garante que a PDE permita acesso de usuario
        if (flags & VMM_PAGE_USER) {
            dir->entries[pd_index] |= VMM_PAGE_USER;
        }
    }

    page_table_t* pt = dir->tables[pd_index];
    pt->entries[pt_index] = (phys_addr & VMM_PAGE_FRAME_MASK) | flags | VMM_PAGE_PRESENT;

    vmm_invlpg(virt_addr);
    return 0;
}

void vmm_unmap_page(page_directory_t* dir, uint32_t virt_addr) {
    if (!dir) dir = current_directory;

    uint32_t pd_index = virt_addr >> 22;
    uint32_t pt_index = (virt_addr >> 12) & 0x03FF;

    if (dir->tables[pd_index] != NULL) {
        page_table_t* pt = dir->tables[pd_index];
        pt->entries[pt_index] = 0;
        vmm_invlpg(virt_addr);
    }
}

uint32_t vmm_get_phys_addr(page_directory_t* dir, uint32_t virt_addr) {
    if (!dir) dir = current_directory;

    uint32_t pd_index = virt_addr >> 22;
    uint32_t pt_index = (virt_addr >> 12) & 0x03FF;
    uint32_t offset   = virt_addr & 0xFFF;

    if (dir->tables[pd_index] == NULL) {
        return 0;
    }

    page_table_t* pt = dir->tables[pd_index];
    if (!(pt->entries[pt_index] & VMM_PAGE_PRESENT)) {
        return 0;
    }

    return (pt->entries[pt_index] & VMM_PAGE_FRAME_MASK) | offset;
}

int vmm_is_mapped(page_directory_t* dir, uint32_t virt_addr) {
    return vmm_get_phys_addr(dir, virt_addr) != 0;
}

void vmm_page_fault_handler(registers_t* regs) {
    uint32_t faulting_address;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(faulting_address));

    int present  = !(regs->err_code & 0x1); // 0 = page not present
    int write    = regs->err_code & 0x2;   // 1 = write operation
    int user     = regs->err_code & 0x4;   // 1 = processor was in user mode (Ring 3)
    int reserved = regs->err_code & 0x8;   // 1 = overwritten CPU-reserved bits

    // Se o Page Fault ocorreu em espaco de usuario (Ring 3), encerra o processo infrator com segfault
    if (user) {
        task_t* cur = task_get_current();
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("\n[SEGMENTATION FAULT] Processo '");
        if (cur) {
            vga_puts(cur->name);
        } else {
            vga_puts("desconhecido");
        }
        vga_puts("' disparou violacao de acesso a memoria no endereco 0x");

        char hex_buf[16];
        itoa((int)faulting_address, hex_buf, 16);
        vga_puts(hex_buf);
        vga_puts(" (");
        if (present) vga_puts("Nao Presente, ");
        else vga_puts("Violacao de Permissao, ");
        if (write) vga_puts("Escrita");
        else vga_puts("Leitura");
        vga_puts(").\nEncerrando processo com segfault.\n");

        task_exit(-1);
        return;
    }

    // Se o Page Fault ocorreu no Kernel (Ring 0), entra em Kernel Panic
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    vga_clear();
    vga_puts("================================================================================\n");
    vga_puts("                     *** KERNEL PANIC: PAGE FAULT (#PF) ***                     \n");
    vga_puts("================================================================================\n\n");

    vga_puts(" Endereco Faltoso (CR2): 0x");
    char hex[16];
    itoa((int)faulting_address, hex, 16);
    vga_puts(hex);
    vga_puts("\n Causa da Falha: ");
    if (present) vga_puts("[Pagina Nao Mapeada] ");
    else vga_puts("[Violacao de Privilegio/Protecao] ");
    if (write) vga_puts("[Acesso de Escrita] ");
    else vga_puts("[Acesso de Leitura] ");
    if (user) vga_puts("[Modo Usuario] ");
    else vga_puts("[Modo Kernel] ");
    if (reserved) vga_puts("[Bits Reservados Sobrescritos]");

    vga_puts("\n\n Dump dos Registradores:\n");
    vga_puts(" EIP: 0x"); itoa((int)regs->eip, hex, 16); vga_puts(hex);
    vga_puts("  CS: 0x"); itoa((int)regs->cs, hex, 16); vga_puts(hex);
    vga_puts("  ESP: 0x"); itoa((int)regs->esp, hex, 16); vga_puts(hex);
    vga_puts("  EFLAGS: 0x"); itoa((int)regs->eflags, hex, 16); vga_puts(hex);
    vga_puts("\n EAX: 0x"); itoa((int)regs->eax, hex, 16); vga_puts(hex);
    vga_puts("  EBX: 0x"); itoa((int)regs->ebx, hex, 16); vga_puts(hex);
    vga_puts("  ECX: 0x"); itoa((int)regs->ecx, hex, 16); vga_puts(hex);
    vga_puts("  EDX: 0x"); itoa((int)regs->edx, hex, 16); vga_puts(hex);

    vga_puts("\n\n Sistema congelado para protecao de integridade da memoria.\n");
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

void vmm_print_info(void) {
    uint32_t cr3_val = 0;
    uint32_t cr0_val = 0;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3_val));
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0_val));

    char buf[32];
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("================ STATUS DA PAGINACAO x86 (VMM) ================\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts(" Paginacao Hardware (CR0.PG bit 31): ");
    if (cr0_val & 0x80000000) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("ATIVADA (4 KB Paging)\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("DESATIVADA\n");
    }

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts(" Diretorio de Paginas Corrente (CR3): 0x");
    itoa((int)cr3_val, buf, 16);
    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_puts(buf);

    if (cr3_val == (uint32_t)&kernel_directory) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts(" (Kernel Directory)\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
        vga_puts(" (User Process Isolated Directory)\n");
    }

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts(" Estrutura: 2 Niveis (Page Directory: 1024 PDEs | Page Table: 1024 PTEs)\n");
    vga_puts(" Mapeamento do Kernel: 0x00000000 - 0x08000000 (128 MB Identidade, Ring 0)\n");
    vga_puts(" Isolamento Ring 3: Bit U/S (Bit 2) nas PDEs/PTEs protege nucleo contra Ring 3\n");
    vga_puts(" Falhas: #PF (ISR 14) isola erros em usuario com SegFault sem derrubar Kernel\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("================================================================\n");
}

