#ifndef KERNEL_KHEAP_H
#define KERNEL_KHEAP_H

#include <libc/stdint.h>
#include <libc/string.h>

#define KHEAP_MAGIC             0x1A2B3C4D
#define KHEAP_START_ADDRESS     0x00200000              // Endereco inicial do Heap (2 MB)
#define KHEAP_INITIAL_SIZE      (8 * 1024 * 1024)       // Tamanho inicial do Heap (8 MB)
#define KHEAP_ALIGNMENT         8                       // Alinhamento obrigatorio de 8 bytes

// Estrutura de cabecalho de cada bloco do Heap
typedef struct kheap_block {
    uint32_t magic;              // Assinatura magica para prevencao de corrupcao de memoria
    size_t size;                 // Tamanho util do bloco em bytes (excluindo este cabecalho)
    int is_free;                 // 1 = Bloco livre, 0 = Bloco em uso
    struct kheap_block* next;    // Proximo bloco na lista duplamente encadeada
    struct kheap_block* prev;    // Bloco anterior na lista duplamente encadeada
} kheap_block_t;

// Inicializa o Heap do Kernel
void  kheap_init(uint32_t start_addr, size_t initial_size);

// Primitivas de alocacao dinamica
void* kmalloc(size_t size);
void  kfree(void* ptr);
void* kcalloc(size_t num, size_t size);
void* krealloc(void* ptr, size_t new_size);

// Funcoes estatisticas do Heap
size_t kheap_get_total_bytes(void);
size_t kheap_get_used_bytes(void);
size_t kheap_get_free_bytes(void);
size_t kheap_get_alloc_count(void);

#endif // KERNEL_KHEAP_H
