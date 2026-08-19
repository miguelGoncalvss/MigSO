#include <fs/migfs.h>
#include <kernel/kheap.h>
#include <libc/string.h>
#include <drivers/ata.h>

static migfs_file_t file_table[MIGFS_MAX_FILES];

// Arquivos embutidos na imagem do migOS carregados durante o boot
static void migfs_load_embedded_files(void) {
    const char* readme_content = 
        "====================================================\n"
        "           BEM-VINDO AO migOS (v0.5)               \n"
        "====================================================\n\n"
        "migOS eh um Sistema Operacional x86 (IA-32) desenvolvido\n"
        "do zero por Miguel para a disciplina de Sistemas Operacionais.\n\n"
        "Principais componentes ativos:\n"
        " [OK] Bootloader MBR (16-bit) -> Protected Mode (32-bit)\n"
        " [OK] GDT (4GB Flat) e IDT (256 Vetores de Interrupcao)\n"
        " [OK] PIC 8259A & PIT 8254 Timer (100 Hz)\n"
        " [OK] Driver de Teclado PS/2 com fila assincrona\n"
        " [OK] Driver de Video VGA 80x25 (Framebuffer 0xB8000)\n"
        " [OK] PMM (Physical Memory Manager - Frames 4KB Bitmap)\n"
        " [OK] KHeap (Alocador de Heap kmalloc / kfree)\n"
        " [OK] RAMDisk / MIGFS (Sistema de Arquivos em Memoria)\n\n"
        "Comandos de arquivos no shell:\n"
        " - ls                   : Lista os arquivos do RAMDisk\n"
        " - cat <arquivo>        : Exibe o conteudo de um arquivo\n"
        " - touch <arquivo>      : Cria um novo arquivo vazio\n"
        " - write <arq> <texto>  : Escreve dados em um arquivo\n"
        " - rm <arquivo>         : Deleta um arquivo\n\n"
        "Digite 'help' para a lista completa de comandos!\n";

    const char* kernel_c_content =
        "/*\n"
        " * migOS - kernel/kernel.c\n"
        " * Ponto de entrada do sistema operacional\n"
        " */\n"
        "#include <kernel/kernel.h>\n"
        "#include <kernel/pmm.h>\n"
        "#include <kernel/kheap.h>\n"
        "#include <fs/migfs.h>\n"
        "#include <shell/shell.h>\n\n"
        "void kernel_main(void) {\n"
        "    vga_init();\n"
        "    idt_init();\n"
        "    isr_init();\n"
        "    pic_remap();\n"
        "    timer_init(100);\n"
        "    keyboard_init();\n\n"
        "    // Gerenciadores de Memoria e Sistema de Arquivos\n"
        "    pmm_init(PMM_DEFAULT_RAM_SIZE);\n"
        "    kheap_init(KHEAP_START_ADDRESS, KHEAP_INITIAL_SIZE);\n"
        "    migfs_init();\n\n"
        "    shell_init();\n"
        "    __asm__ volatile (\"sti\");\n\n"
        "    while (1) {\n"
        "        shell_update();\n"
        "        __asm__ volatile (\"hlt\");\n"
        "    }\n"
        "}\n";

    const char* hello_content =
        "Ola, desenvolvedor! Este arquivo esta armazenado diretamente no\n"
        "RAMDisk (MIGFS) do migOS!\n"
        "Voce pode criar, ler, editar e remover arquivos dinamicamente.\n";

    const char* system_cfg_content =
        "OS_NAME=migOS\n"
        "VERSION=0.5\n"
        "ARCH=x86_IA32_32BIT\n"
        "PIT_FREQUENCY=100Hz\n"
        "PMM_FRAME_SIZE=4096\n"
        "HEAP_BASE=0x00200000\n"
        "HEAP_SIZE_INITIAL=1MB\n"
        "FS_TYPE=MIGFS_RAMDISK\n"
        "AUTHOR=Miguel_Goncalves\n";

    const char* matrix_quote_content =
        "\"Voce toma a pilula azul: a historia acaba e voce acorda\n"
        "na sua cama acreditando no que quiser.\n"
        "Voce toma a pilula vermelha: voce fica no Pais das Maravilhas\n"
        "e eu te mostro ate onde vai a toca do coelho.\"\n"
        " -- Morpheus (migOS Shell: digite 'matrix')\n";

    migfs_create("readme.txt", readme_content, strlen(readme_content), 0);
    migfs_create("kernel.c", kernel_c_content, strlen(kernel_c_content), 0);
    migfs_create("hello.txt", hello_content, strlen(hello_content), 0);
    migfs_create("system.cfg", system_cfg_content, strlen(system_cfg_content), MIGFS_FILE_READONLY);
    migfs_create("secret.txt", matrix_quote_content, strlen(matrix_quote_content), 0);
}

void migfs_init(void) {
    memset(file_table, 0, sizeof(file_table));
    migfs_load_embedded_files();
}

int migfs_create(const char* name, const char* content, size_t size, uint32_t flags) {
    if (!name || name[0] == '\0') return -1;
    if (strlen(name) >= MIGFS_MAX_FILENAME) return -1;

    // Verifica se arquivo com este nome ja existe
    if (migfs_exists(name)) {
        return -2;
    }

    // Localiza um slot livre na tabela de arquivos
    for (size_t i = 0; i < MIGFS_MAX_FILES; i++) {
        if (!file_table[i].in_use) {
            strncpy(file_table[i].name, name, MIGFS_MAX_FILENAME - 1);
            file_table[i].name[MIGFS_MAX_FILENAME - 1] = '\0';
            file_table[i].size = size;
            file_table[i].flags = flags;

            // Aloca buffer no Heap do Kernel via kmalloc
            file_table[i].data = (char*)kmalloc(size + 1);
            if (!file_table[i].data) {
                file_table[i].name[0] = '\0';
                return -3; // Falha de memoria no Heap
            }

            if (content && size > 0) {
                memcpy(file_table[i].data, content, size);
            }
            file_table[i].data[size] = '\0';
            file_table[i].in_use = 1;

            return 0; // Criado com sucesso
        }
    }

    return -4; // Limite maximo de arquivos atingido
}

int migfs_add_buffer(const char* name, char* data, size_t size, uint32_t flags) {
    if (!name || name[0] == '\0' || !data) return -1;
    if (migfs_exists(name)) return -2;

    for (size_t i = 0; i < MIGFS_MAX_FILES; i++) {
        if (!file_table[i].in_use) {
            strncpy(file_table[i].name, name, MIGFS_MAX_FILENAME - 1);
            file_table[i].name[MIGFS_MAX_FILENAME - 1] = '\0';
            file_table[i].size = size;
            file_table[i].flags = flags;
            file_table[i].data = data;
            file_table[i].in_use = 1;
            return 0;
        }
    }
    return -4;
}

#define DOOM_WAD_LBA_START   1025
#define DOOM_WAD_SIZE        4196020
#define DOOM_WAD_SECTORS     8196

int load_doom_wad_from_disk(void) {
    if (migfs_exists("doom1.wad")) {
        return 0; // Já carregado
    }

    if (!ata_is_available()) {
        return -1;
    }

    char* wad_data = (char*)kmalloc(DOOM_WAD_SECTORS * 512);
    if (!wad_data) {
        return -2;
    }

    // Leitura em blocos de 256 setores com barra de progresso
    uint32_t sectors_read = 0;
    uint32_t chunk = 256;

    while (sectors_read < DOOM_WAD_SECTORS) {
        uint32_t to_read = DOOM_WAD_SECTORS - sectors_read;
        if (to_read > chunk) to_read = chunk;

        int ret = ata_read_sectors(DOOM_WAD_LBA_START + sectors_read, to_read, wad_data + (sectors_read * 512));
        if (ret != 0) {
            kfree(wad_data);
            return -3;
        }

        sectors_read += to_read;
    }

    if (memcmp(wad_data, "IWAD", 4) != 0 && memcmp(wad_data, "PWAD", 4) != 0) {
        kfree(wad_data);
        return -4;
    }

    int32_t numlumps = *(int32_t*)(wad_data + 4);
    int32_t infotableofs = *(int32_t*)(wad_data + 8);
    printf("[MIGFS] DOOM1.WAD carregado: %d lumps, offset %d, base=%p\n", numlumps, infotableofs, wad_data);

    migfs_add_buffer("doom1.wad", wad_data, DOOM_WAD_SIZE, MIGFS_FILE_READONLY);
    migfs_add_buffer("DOOM1.WAD", wad_data, DOOM_WAD_SIZE, MIGFS_FILE_READONLY);
    return 0;
}

migfs_file_t* migfs_open(const char* name) {
    if (!name || name[0] == '\0') return NULL;

    if (name[0] == '.' && (name[1] == '/' || name[1] == '\\')) {
        name += 2;
    }

    for (size_t i = 0; i < MIGFS_MAX_FILES; i++) {
        if (file_table[i].in_use && (strcmp(file_table[i].name, name) == 0 || strcasecmp(file_table[i].name, name) == 0)) {
            return &file_table[i];
        }
    }

    return NULL;
}

const char* migfs_read(const char* name) {
    migfs_file_t* f = migfs_open(name);
    return f ? f->data : NULL;
}

int migfs_write(const char* name, const char* content, size_t size) {
    if (!name || name[0] == '\0') return -1;

    migfs_file_t* f = migfs_open(name);
    if (!f) {
        // Se nao existe, cria o arquivo
        return migfs_create(name, content, size, 0);
    }

    if (f->flags & MIGFS_FILE_READONLY) {
        return -2; // Arquivo somente leitura
    }

    char* new_data = (char*)krealloc(f->data, size + 1);
    if (!new_data) {
        return -3; // Falha ao redimensionar buffer no Heap
    }

    f->data = new_data;
    if (content && size > 0) {
        memcpy(f->data, content, size);
    }
    f->data[size] = '\0';
    f->size = size;

    return 0;
}

int migfs_append(const char* name, const char* content, size_t size) {
    if (!name || name[0] == '\0' || size == 0) return 0;

    migfs_file_t* f = migfs_open(name);
    if (!f) {
        return migfs_create(name, content, size, 0);
    }

    if (f->flags & MIGFS_FILE_READONLY) {
        return -2;
    }

    size_t new_size = f->size + size;
    char* new_data = (char*)krealloc(f->data, new_size + 1);
    if (!new_data) {
        return -3;
    }

    f->data = new_data;
    memcpy(f->data + f->size, content, size);
    f->size = new_size;
    f->data[new_size] = '\0';

    return 0;
}

int migfs_delete(const char* name) {
    if (!name || name[0] == '\0') return -1;

    migfs_file_t* f = migfs_open(name);
    if (!f) return -1;

    if (f->flags & MIGFS_FILE_READONLY) {
        return -2; // Nao e permitido deletar arquivo protegido
    }

    if (f->data) {
        kfree(f->data);
        f->data = NULL;
    }

    f->name[0] = '\0';
    f->size = 0;
    f->flags = 0;
    f->in_use = 0;

    return 0;
}

int migfs_exists(const char* name) {
    return migfs_open(name) != NULL;
}

size_t migfs_get_file_count(void) {
    size_t count = 0;
    for (size_t i = 0; i < MIGFS_MAX_FILES; i++) {
        if (file_table[i].in_use) {
            count++;
        }
    }
    return count;
}

size_t migfs_get_total_used_bytes(void) {
    size_t total = 0;
    for (size_t i = 0; i < MIGFS_MAX_FILES; i++) {
        if (file_table[i].in_use) {
            total += file_table[i].size;
        }
    }
    return total;
}

migfs_file_t* migfs_get_file_by_index(size_t index) {
    if (index >= MIGFS_MAX_FILES) return NULL;
    if (file_table[index].in_use) {
        return &file_table[index];
    }
    return NULL;
}
