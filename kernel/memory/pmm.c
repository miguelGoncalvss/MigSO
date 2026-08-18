#include <kernel/pmm.h>
#include <libc/string.h>

// Bitmap estatico em BSS para gerenciar ate 128 MB (4 KB de bitmap)
static uint8_t pmm_bitmap[PMM_BITMAP_SIZE];

static size_t pmm_total_memory = 0;
static size_t pmm_total_blocks = 0;
static size_t pmm_used_blocks  = 0;

static inline void pmm_set_bit(uint32_t bit) {
    pmm_bitmap[bit / 8] |= (uint8_t)(1 << (bit % 8));
}

static inline void pmm_unset_bit(uint32_t bit) {
    pmm_bitmap[bit / 8] &= (uint8_t)~(1 << (bit % 8));
}

static inline int pmm_test_bit(uint32_t bit) {
    return (pmm_bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

// Localiza o primeiro bloco livre (bit 0) no bitmap de forma otimizada
static int pmm_find_first_free_block(void) {
    uint32_t* dwords = (uint32_t*)pmm_bitmap;
    size_t dword_count = pmm_total_blocks / 32;

    for (size_t i = 0; i < dword_count; i++) {
        if (dwords[i] != 0xFFFFFFFF) {
            // Encontrou uma dword com pelo menos 1 bit livre
            for (int j = 0; j < 32; j++) {
                uint32_t bit = (i * 32) + j;
                if (!pmm_test_bit(bit) && bit < pmm_total_blocks) {
                    return (int)bit;
                }
            }
        }
    }

    // Varre eventuais blocos restantes nao alinhados em 32 bits
    for (size_t bit = dword_count * 32; bit < pmm_total_blocks; bit++) {
        if (!pmm_test_bit(bit)) {
            return (int)bit;
        }
    }

    return -1; // Sem blocos livres
}

// Localiza N blocos contiguos livres
static int pmm_find_first_free_blocks(size_t count) {
    if (count == 0) return -1;
    if (count == 1) return pmm_find_first_free_block();

    size_t free_streak = 0;
    size_t start_bit = 0;

    for (size_t bit = 0; bit < pmm_total_blocks; bit++) {
        if (!pmm_test_bit(bit)) {
            if (free_streak == 0) {
                start_bit = bit;
            }
            free_streak++;
            if (free_streak == count) {
                return (int)start_bit;
            }
        } else {
            free_streak = 0;
        }
    }

    return -1;
}

void pmm_init(size_t mem_size) {
    if (mem_size == 0) {
        mem_size = PMM_DEFAULT_RAM_SIZE;
    }
    if (mem_size > PMM_MAX_RAM_SUPPORTED) {
        mem_size = PMM_MAX_RAM_SUPPORTED;
    }

    pmm_total_memory = mem_size;
    pmm_total_blocks = mem_size / PMM_BLOCK_SIZE;

    // Inicialmente marca todos os blocos como OCUPADOS/RESERVADOS (1)
    memset(pmm_bitmap, 0xFF, sizeof(pmm_bitmap));
    pmm_used_blocks = pmm_total_blocks;

    // Libera toda a memoria estendida utilizavel (a partir de 1 MB = 0x100000)
    // O primeiro 1 MB (0x0 - 0x100000) permanece 100% RESERVADO:
    // Protege IVT, BDA, Bootloader (0x7C00), Kernel (0x10000), Stack (0x90000), VGA (0xB8000) e BIOS ROM.
    if (mem_size > 0x00100000) {
        pmm_mark_region_free(0x00100000, mem_size - 0x00100000);
    }
}

void* pmm_alloc_block(void) {
    int free_block = pmm_find_first_free_block();
    if (free_block == -1) {
        return NULL; // Memoria fisica esgotada
    }

    pmm_set_bit((uint32_t)free_block);
    pmm_used_blocks++;

    return (void*)(free_block * PMM_BLOCK_SIZE);
}

void pmm_free_block(void* ptr) {
    if (!ptr) return;

    uint32_t addr = (uint32_t)ptr;
    uint32_t block_index = addr / PMM_BLOCK_SIZE;

    if (block_index >= pmm_total_blocks) return;

    if (pmm_test_bit(block_index)) {
        pmm_unset_bit(block_index);
        if (pmm_used_blocks > 0) {
            pmm_used_blocks--;
        }
    }
}

void* pmm_alloc_blocks(size_t count) {
    if (count == 0) return NULL;
    if (count == 1) return pmm_alloc_block();

    int start_block = pmm_find_first_free_blocks(count);
    if (start_block == -1) {
        return NULL;
    }

    for (size_t i = 0; i < count; i++) {
        pmm_set_bit((uint32_t)(start_block + i));
    }
    pmm_used_blocks += count;

    return (void*)(start_block * PMM_BLOCK_SIZE);
}

void pmm_free_blocks(void* ptr, size_t count) {
    if (!ptr || count == 0) return;

    uint32_t addr = (uint32_t)ptr;
    uint32_t start_block = addr / PMM_BLOCK_SIZE;

    for (size_t i = 0; i < count; i++) {
        uint32_t block_index = start_block + i;
        if (block_index < pmm_total_blocks && pmm_test_bit(block_index)) {
            pmm_unset_bit(block_index);
            if (pmm_used_blocks > 0) {
                pmm_used_blocks--;
            }
        }
    }
}

void pmm_mark_region_used(uint32_t base_addr, size_t size) {
    uint32_t start_block = base_addr / PMM_BLOCK_SIZE;
    uint32_t block_count = (size + PMM_BLOCK_SIZE - 1) / PMM_BLOCK_SIZE;

    for (uint32_t i = 0; i < block_count; i++) {
        uint32_t block_index = start_block + i;
        if (block_index < pmm_total_blocks && !pmm_test_bit(block_index)) {
            pmm_set_bit(block_index);
            pmm_used_blocks++;
        }
    }
}

void pmm_mark_region_free(uint32_t base_addr, size_t size) {
    uint32_t start_block = base_addr / PMM_BLOCK_SIZE;
    uint32_t block_count = size / PMM_BLOCK_SIZE;

    for (uint32_t i = 0; i < block_count; i++) {
        uint32_t block_index = start_block + i;
        if (block_index < pmm_total_blocks && pmm_test_bit(block_index)) {
            pmm_unset_bit(block_index);
            if (pmm_used_blocks > 0) {
                pmm_used_blocks--;
            }
        }
    }
}

size_t pmm_get_total_memory(void) {
    return pmm_total_memory;
}

size_t pmm_get_used_memory(void) {
    return pmm_used_blocks * PMM_BLOCK_SIZE;
}

size_t pmm_get_free_memory(void) {
    return (pmm_total_blocks - pmm_used_blocks) * PMM_BLOCK_SIZE;
}

size_t pmm_get_total_blocks(void) {
    return pmm_total_blocks;
}

size_t pmm_get_used_blocks(void) {
    return pmm_used_blocks;
}

size_t pmm_get_free_blocks(void) {
    return pmm_total_blocks - pmm_used_blocks;
}

size_t pmm_get_block_size(void) {
    return PMM_BLOCK_SIZE;
}
