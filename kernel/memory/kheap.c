#include <kernel/kheap.h>
#include <kernel/pmm.h>
#include <libc/string.h>

#define ALIGN_UP(sz) (((sz) + (KHEAP_ALIGNMENT - 1)) & ~(KHEAP_ALIGNMENT - 1))

static kheap_block_t* head_block = NULL;
static size_t heap_total_size    = 0;
static size_t heap_used_size     = 0;
static size_t heap_alloc_count   = 0;

void kheap_init(uint32_t start_addr, size_t initial_size) {
    if (start_addr == 0) {
        start_addr = KHEAP_START_ADDRESS;
    }
    if (initial_size == 0) {
        initial_size = KHEAP_INITIAL_SIZE;
    }

    // Alinha o tamanho aos blocos de 4 KB
    initial_size = (initial_size + PMM_BLOCK_SIZE - 1) & ~(PMM_BLOCK_SIZE - 1);

    // Marca esta regiao como em uso no PMM para nao haver sobreposicao de frames
    pmm_mark_region_used(start_addr, initial_size);

    heap_total_size  = initial_size;
    heap_used_size   = 0;
    heap_alloc_count = 0;

    // Inicializa o primeiro bloco livre englobando todo o espaco inicial
    head_block = (kheap_block_t*)start_addr;
    head_block->magic   = KHEAP_MAGIC;
    head_block->size    = initial_size - sizeof(kheap_block_t);
    head_block->is_free = 1;
    head_block->next    = NULL;
    head_block->prev    = NULL;
}

// Expande o Heap solicitando blocos contiguos ao PMM
static int kheap_expand(size_t needed_bytes) {
    size_t needed_total = needed_bytes + sizeof(kheap_block_t);
    size_t frames = (needed_total + PMM_BLOCK_SIZE - 1) / PMM_BLOCK_SIZE;
    
    // Expande pelo menos 256 KB (64 frames) de cada vez para evitar fragmentacao excessiva
    if (frames < 64) {
        frames = 64;
    }

    void* new_mem = pmm_alloc_blocks(frames);
    if (!new_mem) {
        return 0; // PMM esgotado
    }

    size_t added_size = frames * PMM_BLOCK_SIZE;
    kheap_block_t* new_block = (kheap_block_t*)new_mem;
    new_block->magic   = KHEAP_MAGIC;
    new_block->size    = added_size - sizeof(kheap_block_t);
    new_block->is_free = 1;
    new_block->next    = NULL;

    // Encontra o ultimo bloco da lista
    kheap_block_t* curr = head_block;
    while (curr && curr->next) {
        curr = curr->next;
    }

    if (curr) {
        curr->next = new_block;
        new_block->prev = curr;

        // Se a nova memoria for imediatamente adjacente, funde os blocos
        if ((uint32_t)curr + sizeof(kheap_block_t) + curr->size == (uint32_t)new_block && curr->is_free) {
            curr->size += sizeof(kheap_block_t) + new_block->size;
            curr->next = NULL;
        }
    } else {
        head_block = new_block;
        new_block->prev = NULL;
    }

    heap_total_size += added_size;
    return 1;
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    size = ALIGN_UP(size);

    kheap_block_t* curr = head_block;

    while (curr) {
        if (curr->magic != KHEAP_MAGIC) {
            return NULL; // Integridade do Heap corrompida
        }

        if (curr->is_free && curr->size >= size) {
            // Divide o bloco se houver espaco suficiente para outro bloco util
            if (curr->size >= size + sizeof(kheap_block_t) + 16) {
                kheap_block_t* next_free = (kheap_block_t*)((uint32_t)curr + sizeof(kheap_block_t) + size);
                next_free->magic   = KHEAP_MAGIC;
                next_free->size    = curr->size - size - sizeof(kheap_block_t);
                next_free->is_free = 1;
                next_free->next    = curr->next;
                next_free->prev    = curr;

                if (curr->next) {
                    curr->next->prev = next_free;
                }
                curr->next = next_free;
                curr->size = size;
            }

            curr->is_free = 0;
            heap_used_size += curr->size;
            heap_alloc_count++;

            return (void*)((uint32_t)curr + sizeof(kheap_block_t));
        }

        curr = curr->next;
    }

    // Nao encontrou espaco: tenta expandir o Heap via PMM
    if (kheap_expand(size)) {
        return kmalloc(size); // Tenta alocar novamente no espaco expandido
    }

    return NULL; // Memoria esgotada
}

void kfree(void* ptr) {
    if (!ptr) return;

    kheap_block_t* block = (kheap_block_t*)((uint32_t)ptr - sizeof(kheap_block_t));

    if (block->magic != KHEAP_MAGIC) {
        return; // Ponteiro invalido ou cabecalho corrompido
    }

    if (block->is_free) {
        return; // Evita Double Free
    }

    block->is_free = 1;

    if (heap_used_size >= block->size) {
        heap_used_size -= block->size;
    } else {
        heap_used_size = 0;
    }

    if (heap_alloc_count > 0) {
        heap_alloc_count--;
    }

    // Coalescencia com o proximo bloco (se livre e adjacente)
    if (block->next && block->next->magic == KHEAP_MAGIC && block->next->is_free) {
        if ((uint32_t)block + sizeof(kheap_block_t) + block->size == (uint32_t)block->next) {
            block->size += sizeof(kheap_block_t) + block->next->size;
            block->next = block->next->next;
            if (block->next) {
                block->next->prev = block;
            }
        }
    }

    // Coalescencia com o bloco anterior (se livre e adjacente)
    if (block->prev && block->prev->magic == KHEAP_MAGIC && block->prev->is_free) {
        if ((uint32_t)block->prev + sizeof(kheap_block_t) + block->prev->size == (uint32_t)block) {
            block->prev->size += sizeof(kheap_block_t) + block->size;
            block->prev->next = block->next;
            if (block->next) {
                block->next->prev = block->prev;
            }
        }
    }
}

void* kcalloc(size_t num, size_t size) {
    size_t total = num * size;
    void* ptr = kmalloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void* krealloc(void* ptr, size_t new_size) {
    if (!ptr) {
        return kmalloc(new_size);
    }
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    kheap_block_t* block = (kheap_block_t*)((uint32_t)ptr - sizeof(kheap_block_t));
    if (block->magic != KHEAP_MAGIC) {
        return NULL;
    }

    if (block->size >= new_size) {
        return ptr;
    }

    void* new_ptr = kmalloc(new_size);
    if (!new_ptr) return NULL;

    size_t copy_size = (block->size < new_size) ? block->size : new_size;
    memcpy(new_ptr, ptr, copy_size);
    kfree(ptr);

    return new_ptr;
}

size_t kheap_get_total_bytes(void) {
    return heap_total_size;
}

size_t kheap_get_used_bytes(void) {
    return heap_used_size;
}

size_t kheap_get_free_bytes(void) {
    if (heap_total_size >= heap_used_size) {
        return heap_total_size - heap_used_size;
    }
    return 0;
}

size_t kheap_get_alloc_count(void) {
    return heap_alloc_count;
}
