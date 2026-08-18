#ifndef KERNEL_PMM_H
#define KERNEL_PMM_H

#include <libc/stdint.h>
#include <libc/string.h>

#define PMM_BLOCK_SIZE          4096                    // Tamanho de cada frame/bloco físico (4 KB)
#define PMM_BLOCKS_PER_BYTE     8                       // 8 blocos representados por byte no bitmap
#define PMM_DEFAULT_RAM_SIZE    (64 * 1024 * 1024)      // 64 MB de memória física padrão (QEMU)
#define PMM_MAX_RAM_SUPPORTED   (128 * 1024 * 1024)     // Suporte a até 128 MB (bitmap de 4 KB)
#define PMM_MAX_BLOCKS          (PMM_MAX_RAM_SUPPORTED / PMM_BLOCK_SIZE)
#define PMM_BITMAP_SIZE         (PMM_MAX_BLOCKS / PMM_BLOCKS_PER_BYTE)

// Inicializa o gerenciador de memória física
void pmm_init(size_t mem_size);

// Alocação e desalocação de 1 bloco (4 KB)
void* pmm_alloc_block(void);
void  pmm_free_block(void* ptr);

// Alocação e desalocação de múltiplos blocos contíguos
void* pmm_alloc_blocks(size_t count);
void  pmm_free_blocks(void* ptr, size_t count);

// Marcação direta de regiões físicas de memória
void  pmm_mark_region_used(uint32_t base_addr, size_t size);
void  pmm_mark_region_free(uint32_t base_addr, size_t size);

// Funções informativas e estatísticas
size_t pmm_get_total_memory(void);
size_t pmm_get_used_memory(void);
size_t pmm_get_free_memory(void);
size_t pmm_get_total_blocks(void);
size_t pmm_get_used_blocks(void);
size_t pmm_get_free_blocks(void);
size_t pmm_get_block_size(void);

#endif // KERNEL_PMM_H
