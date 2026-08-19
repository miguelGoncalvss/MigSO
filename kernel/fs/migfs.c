#include <fs/migfs.h>
#include <kernel/kheap.h>
#include <libc/string.h>
#include <libc/stdio.h>
#include <drivers/ata.h>

static migfs_file_t file_table[MIGFS_MAX_FILES];
static int migfs_is_disk_backed = 0;

int migfs_is_persisted(void) {
    return migfs_is_disk_backed;
}

// Sincroniza todos os arquivos da memoria RAM com o disco ATA (LBA 1025+)
int migfs_sync_to_disk(void) {
    if (!ata_is_available()) {
        return -1;
    }

    static migfs_disk_entry_t sync_entries[MIGFS_MAX_FILES];
    memset(sync_entries, 0, sizeof(sync_entries));

    uint32_t current_lba = MIGFS_DATA_START_LBA;
    uint32_t active_count = 0;

    for (size_t i = 0; i < MIGFS_MAX_FILES; i++) {
        if (file_table[i].in_use && file_table[i].name[0] != '\0') {
            strncpy(sync_entries[i].name, file_table[i].name, MIGFS_MAX_FILENAME - 1);
            sync_entries[i].name[MIGFS_MAX_FILENAME - 1] = '\0';
            sync_entries[i].size = file_table[i].size;
            sync_entries[i].flags = file_table[i].flags;
            sync_entries[i].in_use = 1;

            uint32_t sectors = (file_table[i].size + 511) / 512;
            if (sectors == 0 && file_table[i].size > 0) sectors = 1;

            sync_entries[i].sector_count = sectors;
            sync_entries[i].start_sector = current_lba;

            if (sectors > 0 && file_table[i].data) {
                for (uint32_t s = 0; s < sectors; s++) {
                    char sec_buf[512];
                    memset(sec_buf, 0, 512);
                    size_t bytes_to_copy = 512;
                    if (s * 512 + bytes_to_copy > file_table[i].size) {
                        bytes_to_copy = file_table[i].size - (s * 512);
                    }
                    memcpy(sec_buf, file_table[i].data + (s * 512), bytes_to_copy);
                    ata_write_sectors(current_lba + s, 1, sec_buf);
                }
            }

            current_lba += (sectors > 0) ? sectors : 1;
            active_count++;
        }
    }

    if (ata_write_sectors(MIGFS_FILE_TABLE_LBA, MIGFS_FILE_TABLE_SECTORS, sync_entries) != 0) {
        return -2;
    }

    // Grava o superbloco no setor LBA 1025
    migfs_superblock_t sb;
    memset(&sb, 0, sizeof(sb));
    sb.magic = MIGFS_MAGIC;
    sb.version = MIGFS_VERSION;
    sb.file_count = active_count;
    sb.next_free_lba = current_lba;
    sb.total_sectors = 32000;
    strncpy(sb.label, "migOS_PERSISTENT_HD", 31);
    sb.label[31] = '\0';

    if (ata_write_sectors(MIGFS_SUPER_LBA, 1, &sb) != 0) {
        return -3;
    }
    ata_flush();

    migfs_is_disk_backed = 1;
    return 0;
}

// Carrega os arquivos persistidos do disco ATA para a memoria
int migfs_load_from_disk(void) {
    if (!ata_is_available()) {
        return -1;
    }

    migfs_superblock_t sb;
    memset(&sb, 0, sizeof(sb));
    if (ata_read_sectors(MIGFS_SUPER_LBA, 1, &sb) != 0) {
        return -2;
    }

    if (sb.magic != MIGFS_MAGIC) {
        return -3; // Superbloco invalido ou disco virgem
    }

    static migfs_disk_entry_t load_entries[MIGFS_MAX_FILES];
    memset(load_entries, 0, sizeof(load_entries));
    if (ata_read_sectors(MIGFS_FILE_TABLE_LBA, MIGFS_FILE_TABLE_SECTORS, load_entries) != 0) {
        return -4;
    }

    // Limpa tabela em memoria antes de preencher
    for (size_t i = 0; i < MIGFS_MAX_FILES; i++) {
        if (file_table[i].in_use && file_table[i].data) {
            kfree(file_table[i].data);
        }
        memset(&file_table[i], 0, sizeof(migfs_file_t));
    }

    for (size_t i = 0; i < MIGFS_MAX_FILES; i++) {
        if (load_entries[i].in_use && load_entries[i].name[0] != '\0') {
            strncpy(file_table[i].name, load_entries[i].name, MIGFS_MAX_FILENAME - 1);
            file_table[i].name[MIGFS_MAX_FILENAME - 1] = '\0';
            file_table[i].size = load_entries[i].size;
            file_table[i].flags = load_entries[i].flags;
            file_table[i].in_use = 1;

            file_table[i].data = (char*)kmalloc(load_entries[i].size + 1);
            if (!file_table[i].data) {
                continue;
            }

            if (load_entries[i].size > 0 && load_entries[i].sector_count > 0) {
                for (uint32_t s = 0; s < load_entries[i].sector_count; s++) {
                    char sec_buf[512];
                    if (ata_read_sectors(load_entries[i].start_sector + s, 1, sec_buf) == 0) {
                        size_t bytes_to_copy = 512;
                        if (s * 512 + bytes_to_copy > load_entries[i].size) {
                            bytes_to_copy = load_entries[i].size - (s * 512);
                        }
                        memcpy(file_table[i].data + (s * 512), sec_buf, bytes_to_copy);
                    }
                }
            }
            file_table[i].data[load_entries[i].size] = '\0';
        }
    }

    migfs_is_disk_backed = 1;
    return 0;
}

// Arquivos embutidos na imagem do migOS carregados no primeiro boot
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
        " [OK] Driver de Teclado PS/2 (Shift, Caps, Setas, Modificadores)\n"
        " [OK] Driver de Video VGA 80x25 / BGA 640x480 True Color\n"
        " [OK] PMM (Physical Memory Manager - Frames 4KB Bitmap)\n"
        " [OK] KHeap (Alocador de Heap kmalloc / kfree)\n"
        " [OK] MIGFS Persistente com Gravacao em Disco ATA/IDE\n"
        " [OK] Editor de Texto Visual (CLI: 'edit'/'nano' & GUI: 'TextEdit')\n"
        " [OK] Interpretador e Executor de Scripts .txt ('run'/'exec')\n\n"
        "Comandos de arquivos no shell:\n"
        " - ls                   : Lista os arquivos do disco\n"
        " - cat <arquivo>        : Exibe o conteudo de um arquivo\n"
        " - edit / nano <arq>    : Abre o Editor de Texto no Terminal\n"
        " - run / exec <arq.txt> : Executa script ou interpretador .txt\n"
        " - touch <arquivo>      : Cria um novo arquivo vazio\n"
        " - write <arq> <texto>  : Escreve dados em um arquivo\n"
        " - sync                 : Sincroniza dados com o disco ATA\n"
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
        "Ola, desenvolvedor! Este arquivo esta armazenado e persistido\n"
        "no disco ATA primario (MIGFS) do migOS!\n"
        "Voce pode criar, ler, editar e remover arquivos dinamicamente,\n"
        "tanto pelo terminal (edit/nano) quanto pela interface grafica!\n";

    const char* system_cfg_content =
        "OS_NAME=migOS\n"
        "VERSION=0.5\n"
        "ARCH=x86_IA32_32BIT\n"
        "PIT_FREQUENCY=100Hz\n"
        "PMM_FRAME_SIZE=4096\n"
        "HEAP_BASE=0x00200000\n"
        "HEAP_SIZE_INITIAL=1MB\n"
        "FS_TYPE=MIGFS_ATA_PERSISTENT\n"
        "AUTHOR=Miguel_Goncalves\n";

    const char* matrix_quote_content =
        "\"Voce toma a pilula azul: a historia acaba e voce acorda\n"
        "na sua cama acreditando no que quiser.\n"
        "Voce toma a pilula vermelha: voce fica no Pais das Maravilhas\n"
        "e eu te mostro ate onde vai a toca do coelho.\"\n"
        " -- Morpheus (migOS Shell: digite 'matrix')\n";

    const char* demo_script_content =
        "# Script Demonstrativo do migOS\n"
        "# Interpretador de Scripts .txt com Operacoes Aritmeticas e Variaveis\n"
        "echo ====================================================\n"
        "echo    Iniciando Demonstracao do migOS Script Engine   \n"
        "echo ====================================================\n"
        "echo Sistema: migOS IA-32 Bare-Metal Edition\n"
        "set USER=Miguel\n"
        "echo Bem-vindo ao interpretador de scripts, $USER!\n"
        "echo.\n"
        "echo Executando calculos matematicos no interpretador:\n"
        "calc 15 + 25 * 2\n"
        "calc (100 - 20) / 4\n"
        "calc 256 * 16\n"
        "echo.\n"
        "echo Verificando memoria do sistema:\n"
        "meminfo\n"
        "echo.\n"
        "echo Listando arquivos salvos no disco persistente:\n"
        "ls\n"
        "echo.\n"
        "echo [SUCESSO] Script executado com 100% de exito!\n";

    const char* calc_script_content =
        "# Calculadora e Interpretador Matematico migOS\n"
        "echo [CALC] Testando operacoes aritmeticas com precedencia:\n"
        "calc 10 + 20\n"
        "calc 50 - 18\n"
        "calc 12 * 12\n"
        "calc 1024 / 16\n"
        "calc (10 + 5) * (8 - 2)\n"
        "calc 256 + 512 + 1024\n"
        "echo [OK] Todos os calculos foram interpretados e exibidos com sucesso!\n";

    migfs_create("readme.txt", readme_content, strlen(readme_content), 0);
    migfs_create("kernel.c", kernel_c_content, strlen(kernel_c_content), 0);
    migfs_create("hello.txt", hello_content, strlen(hello_content), 0);
    migfs_create("system.cfg", system_cfg_content, strlen(system_cfg_content), MIGFS_FILE_READONLY);
    migfs_create("secret.txt", matrix_quote_content, strlen(matrix_quote_content), 0);
    migfs_create("demo.txt", demo_script_content, strlen(demo_script_content), 0);
    migfs_create("calc.txt", calc_script_content, strlen(calc_script_content), 0);

    // Pastas padrao e arquivos de exemplo em subdiretorios
    migfs_mkdir("docs");
    const char* manual_txt = "Manual migOS:\n- ls / cd / pwd / mkdir / rmdir / mv / cp\n- edit / nano / run / calc / gui\n";
    migfs_create("docs/manual.txt", manual_txt, strlen(manual_txt), 0);

    migfs_mkdir("scripts");
    const char* sub_script = "# Script em Subpasta\necho [OK] Executando dentro de /scripts!\ncalc 12 * 8\n";
    migfs_create("scripts/teste.txt", sub_script, strlen(sub_script), 0);
}

void migfs_init(void) {
    memset(file_table, 0, sizeof(file_table));

    // Inicializa disco ATA primario
    ata_init();

    // Tenta carregar os arquivos existentes do disco
    int ret = migfs_load_from_disk();
    if (ret != 0 || migfs_get_file_count() == 0) {
        // Se o disco ainda nao foi formatado, cria os arquivos padrao e salva no disco
        migfs_load_embedded_files();
        migfs_sync_to_disk();
    }
}

int migfs_format_disk(void) {
    memset(file_table, 0, sizeof(file_table));
    migfs_load_embedded_files();
    return migfs_sync_to_disk();
}

void migfs_path_normalize(const char* path, char* out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!path || path[0] == '\0') {
        strncpy(out, "/", out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }

    char temp[MIGFS_MAX_FILENAME * 2];
    size_t ti = 0;

    for (size_t i = 0; path[i] != '\0' && ti < sizeof(temp) - 2; i++) {
        char c = path[i];
        if (c == '\\') c = '/';
        if (c == '/' && ti > 0 && temp[ti - 1] == '/') {
            continue;
        }
        temp[ti++] = c;
    }
    temp[ti] = '\0';

    char segments[8][MIGFS_MAX_FILENAME];
    int seg_count = 0;

    char* p = temp;
    if (*p == '/') p++;

    while (*p != '\0') {
        char* start = p;
        while (*p != '/' && *p != '\0') p++;
        size_t len = p - start;
        if (len > 0) {
            char seg[MIGFS_MAX_FILENAME];
            if (len >= MIGFS_MAX_FILENAME) len = MIGFS_MAX_FILENAME - 1;
            strncpy(seg, start, len);
            seg[len] = '\0';

            if (strcmp(seg, ".") == 0) {
                // ignora
            } else if (strcmp(seg, "..") == 0) {
                if (seg_count > 0) seg_count--;
            } else {
                if (seg_count < 8) {
                    strncpy(segments[seg_count++], seg, MIGFS_MAX_FILENAME - 1);
                }
            }
        }
        if (*p == '/') p++;
    }

    if (seg_count == 0) {
        strncpy(out, "/", out_size - 1);
        out[out_size - 1] = '\0';
    } else {
        out[0] = '\0';
        for (int i = 0; i < seg_count; i++) {
            size_t cur = strlen(out);
            if (cur + 1 + strlen(segments[i]) < out_size) {
                out[cur] = '/';
                out[cur + 1] = '\0';
                strcat(out, segments[i]);
            }
        }
    }
}

void migfs_path_combine(const char* base, const char* rel, char* out, size_t out_size) {
    if (!out || out_size == 0) return;
    if (!rel || rel[0] == '\0') {
        migfs_path_normalize(base, out, out_size);
        return;
    }

    if (rel[0] == '/' || rel[0] == '\\') {
        migfs_path_normalize(rel, out, out_size);
        return;
    }

    char joined[MIGFS_MAX_FILENAME * 2];
    joined[0] = '\0';

    if (base && base[0] != '\0' && strcmp(base, "/") != 0) {
        strncpy(joined, base, sizeof(joined) - 1);
        joined[sizeof(joined) - 1] = '\0';
        size_t len = strlen(joined);
        if (len > 0 && joined[len - 1] != '/') {
            strcat(joined, "/");
        }
    } else {
        strcpy(joined, "/");
    }

    strncat(joined, rel, sizeof(joined) - strlen(joined) - 1);
    migfs_path_normalize(joined, out, out_size);
}

void migfs_get_parent_dir(const char* path, char* out_parent, size_t out_size) {
    if (!out_parent || out_size == 0) return;
    char norm[MIGFS_MAX_FILENAME];
    migfs_path_normalize(path, norm, sizeof(norm));

    if (strcmp(norm, "/") == 0) {
        strncpy(out_parent, "/", out_size - 1);
        out_parent[out_size - 1] = '\0';
        return;
    }

    char* last_slash = strrchr(norm, '/');
    if (!last_slash || last_slash == norm) {
        strncpy(out_parent, "/", out_size - 1);
        out_parent[out_size - 1] = '\0';
    } else {
        *last_slash = '\0';
        strncpy(out_parent, norm, out_size - 1);
        out_parent[out_size - 1] = '\0';
    }
}

int migfs_create(const char* name, const char* content, size_t size, uint32_t flags) {
    if (!name || name[0] == '\0') return -1;

    char clean_name[MIGFS_MAX_FILENAME];
    migfs_path_normalize(name, clean_name, sizeof(clean_name));

    const char* target = clean_name;
    if (target[0] == '/') target++;
    if (target[0] == '\0') return -1;

    // Se ja existe, sobrescreve o conteudo
    migfs_file_t* existing = migfs_open(target);
    if (existing) {
        return migfs_write(target, content, size);
    }

    // Localiza um slot livre na tabela de arquivos
    for (size_t i = 0; i < MIGFS_MAX_FILES; i++) {
        if (!file_table[i].in_use) {
            strncpy(file_table[i].name, target, MIGFS_MAX_FILENAME - 1);
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

            // Persiste automaticamente no disco
            migfs_sync_to_disk();
            return 0;
        }
    }

    return -4; // Limite maximo de arquivos atingido
}

int migfs_add_buffer(const char* name, char* data, size_t size, uint32_t flags) {
    if (!name || name[0] == '\0' || !data) return -1;
    if (migfs_exists(name)) return -2;

    const char* target = name;
    if (target[0] == '/') target++;

    for (size_t i = 0; i < MIGFS_MAX_FILES; i++) {
        if (!file_table[i].in_use) {
            strncpy(file_table[i].name, target, MIGFS_MAX_FILENAME - 1);
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

#define DOOM_WAD_LBA_START   5000
#define DOOM_WAD_SIZE        4196020
#define DOOM_WAD_SECTORS     8196

int load_doom_wad_from_disk(void) {
    if (migfs_exists("doom1.wad")) {
        return 0; // Ja carregado
    }

    if (!ata_is_available()) {
        return -1;
    }

    char* wad_data = (char*)kmalloc(DOOM_WAD_SECTORS * 512);
    if (!wad_data) {
        return -2;
    }

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

    migfs_add_buffer("doom1.wad", wad_data, DOOM_WAD_SIZE, MIGFS_FILE_READONLY);
    migfs_add_buffer("DOOM1.WAD", wad_data, DOOM_WAD_SIZE, MIGFS_FILE_READONLY);
    return 0;
}

migfs_file_t* migfs_open(const char* name) {
    if (!name || name[0] == '\0') return NULL;

    char clean_name[MIGFS_MAX_FILENAME];
    migfs_path_normalize(name, clean_name, sizeof(clean_name));

    const char* target = clean_name;
    if (target[0] == '/') target++;
    if (target[0] == '\0') return NULL;

    for (size_t i = 0; i < MIGFS_MAX_FILES; i++) {
        if (file_table[i].in_use) {
            const char* fname = file_table[i].name;
            if (fname[0] == '/') fname++;

            if (strcmp(fname, target) == 0 || strcasecmp(fname, target) == 0) {
                return &file_table[i];
            }
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

    // Persiste automaticamente no disco ATA
    migfs_sync_to_disk();
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

    migfs_sync_to_disk();
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

    migfs_sync_to_disk();
    return 0;
}

int migfs_exists(const char* name) {
    return migfs_open(name) != NULL;
}

int migfs_is_dir(const char* path) {
    if (!path || path[0] == '\0') return 1;

    char clean_name[MIGFS_MAX_FILENAME];
    migfs_path_normalize(path, clean_name, sizeof(clean_name));

    if (strcmp(clean_name, "/") == 0) return 1;

    const char* target = clean_name;
    if (target[0] == '/') target++;

    for (size_t i = 0; i < MIGFS_MAX_FILES; i++) {
        if (file_table[i].in_use) {
            const char* fname = file_table[i].name;
            if (fname[0] == '/') fname++;

            if ((strcmp(fname, target) == 0 || strcasecmp(fname, target) == 0) &&
                (file_table[i].flags & MIGFS_FILE_DIRECTORY)) {
                return 1;
            }

            size_t tlen = strlen(target);
            if (strncasecmp(fname, target, tlen) == 0 && fname[tlen] == '/') {
                return 1;
            }
        }
    }

    return 0;
}

int migfs_mkdir(const char* path) {
    if (!path || path[0] == '\0') return -1;

    char clean_name[MIGFS_MAX_FILENAME];
    migfs_path_normalize(path, clean_name, sizeof(clean_name));

    const char* target = clean_name;
    if (target[0] == '/') target++;
    if (target[0] == '\0') return -1;

    if (migfs_exists(target) || migfs_is_dir(target)) {
        return -2; // Ja existe
    }

    for (size_t i = 0; i < MIGFS_MAX_FILES; i++) {
        if (!file_table[i].in_use) {
            strncpy(file_table[i].name, target, MIGFS_MAX_FILENAME - 1);
            file_table[i].name[MIGFS_MAX_FILENAME - 1] = '\0';
            file_table[i].size = 0;
            file_table[i].data = NULL;
            file_table[i].flags = MIGFS_FILE_DIRECTORY;
            file_table[i].in_use = 1;

            migfs_sync_to_disk();
            return 0;
        }
    }

    return -4;
}

int migfs_rmdir(const char* path) {
    if (!path || path[0] == '\0') return -1;

    char clean_name[MIGFS_MAX_FILENAME];
    migfs_path_normalize(path, clean_name, sizeof(clean_name));

    const char* target = clean_name;
    if (target[0] == '/') target++;
    if (target[0] == '\0') return -1;

    size_t tlen = strlen(target);

    // Verifica se ha arquivos dentro da pasta
    for (size_t i = 0; i < MIGFS_MAX_FILES; i++) {
        if (file_table[i].in_use) {
            const char* fname = file_table[i].name;
            if (fname[0] == '/') fname++;

            if (strncasecmp(fname, target, tlen) == 0 && fname[tlen] == '/') {
                return -2; // Diretorio nao vazio
            }
        }
    }

    for (size_t i = 0; i < MIGFS_MAX_FILES; i++) {
        if (file_table[i].in_use) {
            const char* fname = file_table[i].name;
            if (fname[0] == '/') fname++;

            if ((strcmp(fname, target) == 0 || strcasecmp(fname, target) == 0) &&
                (file_table[i].flags & MIGFS_FILE_DIRECTORY)) {
                if (file_table[i].data) {
                    kfree(file_table[i].data);
                    file_table[i].data = NULL;
                }
                file_table[i].name[0] = '\0';
                file_table[i].size = 0;
                file_table[i].flags = 0;
                file_table[i].in_use = 0;

                migfs_sync_to_disk();
                return 0;
            }
        }
    }

    return -3;
}

int migfs_move(const char* src, const char* dest) {
    if (!src || !dest || src[0] == '\0' || dest[0] == '\0') return -1;

    migfs_file_t* sf = migfs_open(src);
    if (!sf) return -1;

    if (sf->flags & MIGFS_FILE_READONLY) return -2;

    char clean_dest[MIGFS_MAX_FILENAME];
    migfs_path_normalize(dest, clean_dest, sizeof(clean_dest));

    const char* d_target = clean_dest;
    if (d_target[0] == '/') d_target++;

    char new_path[MIGFS_MAX_FILENAME];

    if (migfs_is_dir(clean_dest)) {
        const char* base_name = sf->name;
        char* last_slash = strrchr(sf->name, '/');
        if (last_slash) base_name = last_slash + 1;

        if (d_target[0] == '\0') {
            strncpy(new_path, base_name, MIGFS_MAX_FILENAME - 1);
        } else {
            strncpy(new_path, d_target, MIGFS_MAX_FILENAME - 1);
            new_path[MIGFS_MAX_FILENAME - 1] = '\0';
            strncat(new_path, "/", MIGFS_MAX_FILENAME - strlen(new_path) - 1);
            strncat(new_path, base_name, MIGFS_MAX_FILENAME - strlen(new_path) - 1);
        }
    } else {
        strncpy(new_path, d_target, MIGFS_MAX_FILENAME - 1);
    }
    new_path[MIGFS_MAX_FILENAME - 1] = '\0';

    if (sf->flags & MIGFS_FILE_DIRECTORY) {
        char old_prefix[MIGFS_MAX_FILENAME];
        char new_prefix[MIGFS_MAX_FILENAME];
        snprintf(old_prefix, sizeof(old_prefix), "%s/", sf->name);
        snprintf(new_prefix, sizeof(new_prefix), "%s/", new_path);

        size_t old_plen = strlen(old_prefix);
        for (size_t i = 0; i < MIGFS_MAX_FILES; i++) {
            if (file_table[i].in_use && &file_table[i] != sf) {
                if (strncmp(file_table[i].name, old_prefix, old_plen) == 0) {
                    char child_new[MIGFS_MAX_FILENAME];
                    snprintf(child_new, sizeof(child_new), "%s%s", new_prefix, file_table[i].name + old_plen);
                    strncpy(file_table[i].name, child_new, MIGFS_MAX_FILENAME - 1);
                    file_table[i].name[MIGFS_MAX_FILENAME - 1] = '\0';
                }
            }
        }
    }

    strncpy(sf->name, new_path, MIGFS_MAX_FILENAME - 1);
    sf->name[MIGFS_MAX_FILENAME - 1] = '\0';

    migfs_sync_to_disk();
    return 0;
}

int migfs_copy(const char* src, const char* dest) {
    if (!src || !dest || src[0] == '\0' || dest[0] == '\0') return -1;

    migfs_file_t* sf = migfs_open(src);
    if (!sf) return -1;

    char clean_dest[MIGFS_MAX_FILENAME];
    migfs_path_normalize(dest, clean_dest, sizeof(clean_dest));

    const char* d_target = clean_dest;
    if (d_target[0] == '/') d_target++;

    char new_path[MIGFS_MAX_FILENAME];

    if (migfs_is_dir(clean_dest)) {
        const char* base_name = sf->name;
        char* last_slash = strrchr(sf->name, '/');
        if (last_slash) base_name = last_slash + 1;

        if (d_target[0] == '\0') {
            strncpy(new_path, base_name, MIGFS_MAX_FILENAME - 1);
        } else {
            strncpy(new_path, d_target, MIGFS_MAX_FILENAME - 1);
            new_path[MIGFS_MAX_FILENAME - 1] = '\0';
            strncat(new_path, "/", MIGFS_MAX_FILENAME - strlen(new_path) - 1);
            strncat(new_path, base_name, MIGFS_MAX_FILENAME - strlen(new_path) - 1);
        }
    } else {
        strncpy(new_path, d_target, MIGFS_MAX_FILENAME - 1);
    }
    new_path[MIGFS_MAX_FILENAME - 1] = '\0';

    if (sf->flags & MIGFS_FILE_DIRECTORY) {
        return migfs_mkdir(new_path);
    } else {
        return migfs_create(new_path, sf->data, sf->size, sf->flags & ~MIGFS_FILE_READONLY);
    }
}

int migfs_get_dir_items(const char* dir_path, migfs_dir_item_t* items, size_t max_items, size_t* out_count) {
    if (!items || max_items == 0 || !out_count) return -1;
    *out_count = 0;

    char clean_dir[MIGFS_MAX_FILENAME];
    migfs_path_normalize(dir_path, clean_dir, sizeof(clean_dir));

    const char* target = clean_dir;
    if (target[0] == '/') target++;
    size_t target_len = strlen(target);

    size_t count = 0;

    // Adiciona ".." para subir de nivel caso nao esteja na raiz
    if (strcmp(clean_dir, "/") != 0 && count < max_items) {
        strncpy(items[count].name, "..", MIGFS_MAX_FILENAME - 1);
        items[count].name[MIGFS_MAX_FILENAME - 1] = '\0';
        char parent[MIGFS_MAX_FILENAME];
        migfs_get_parent_dir(clean_dir, parent, sizeof(parent));
        strncpy(items[count].full_path, parent, MIGFS_MAX_FILENAME - 1);
        items[count].full_path[MIGFS_MAX_FILENAME - 1] = '\0';
        items[count].size = 0;
        items[count].flags = MIGFS_FILE_DIRECTORY;
        items[count].is_dir = 1;
        count++;
    }

    for (size_t i = 0; i < MIGFS_MAX_FILES && count < max_items; i++) {
        if (!file_table[i].in_use) continue;

        const char* fname = file_table[i].name;
        if (fname[0] == '/') fname++;
        if (fname[0] == '\0') continue;

        size_t flen = strlen(fname);

        if (target_len == 0) {
            // Listagem da raiz: pega arquivos sem barra ou captura pastas de topo
            char* slash = strchr(fname, '/');
            if (!slash) {
                strncpy(items[count].name, fname, MIGFS_MAX_FILENAME - 1);
                items[count].name[MIGFS_MAX_FILENAME - 1] = '\0';
                migfs_path_combine(clean_dir, fname, items[count].full_path, sizeof(items[count].full_path));
                items[count].size = file_table[i].size;
                items[count].flags = file_table[i].flags;
                items[count].is_dir = (file_table[i].flags & MIGFS_FILE_DIRECTORY) ? 1 : 0;
                count++;
            } else {
                size_t dlen = slash - fname;
                char dirname[MIGFS_MAX_FILENAME];
                if (dlen >= MIGFS_MAX_FILENAME) dlen = MIGFS_MAX_FILENAME - 1;
                strncpy(dirname, fname, dlen);
                dirname[dlen] = '\0';

                int already = 0;
                for (size_t k = 0; k < count; k++) {
                    if (strcmp(items[k].name, dirname) == 0) {
                        already = 1;
                        break;
                    }
                }
                if (!already && count < max_items) {
                    strncpy(items[count].name, dirname, MIGFS_MAX_FILENAME - 1);
                    items[count].name[MIGFS_MAX_FILENAME - 1] = '\0';
                    migfs_path_combine(clean_dir, dirname, items[count].full_path, sizeof(items[count].full_path));
                    items[count].size = 0;
                    items[count].flags = MIGFS_FILE_DIRECTORY;
                    items[count].is_dir = 1;
                    count++;
                }
            }
        } else {
            // Listagem de subdiretorio: ex: target = "docs"
            if (flen > target_len && strncasecmp(fname, target, target_len) == 0 && fname[target_len] == '/') {
                const char* rel = fname + target_len + 1;
                char* slash = strchr(rel, '/');
                if (!slash) {
                    strncpy(items[count].name, rel, MIGFS_MAX_FILENAME - 1);
                    items[count].name[MIGFS_MAX_FILENAME - 1] = '\0';
                    migfs_path_combine(clean_dir, rel, items[count].full_path, sizeof(items[count].full_path));
                    items[count].size = file_table[i].size;
                    items[count].flags = file_table[i].flags;
                    items[count].is_dir = (file_table[i].flags & MIGFS_FILE_DIRECTORY) ? 1 : 0;
                    count++;
                } else {
                    size_t dlen = slash - rel;
                    char dirname[MIGFS_MAX_FILENAME];
                    if (dlen >= MIGFS_MAX_FILENAME) dlen = MIGFS_MAX_FILENAME - 1;
                    strncpy(dirname, rel, dlen);
                    dirname[dlen] = '\0';

                    int already = 0;
                    for (size_t k = 0; k < count; k++) {
                        if (strcmp(items[k].name, dirname) == 0) {
                            already = 1;
                            break;
                        }
                    }
                    if (!already && count < max_items) {
                        strncpy(items[count].name, dirname, MIGFS_MAX_FILENAME - 1);
                        items[count].name[MIGFS_MAX_FILENAME - 1] = '\0';
                        migfs_path_combine(clean_dir, dirname, items[count].full_path, sizeof(items[count].full_path));
                        items[count].size = 0;
                        items[count].flags = MIGFS_FILE_DIRECTORY;
                        items[count].is_dir = 1;
                        count++;
                    }
                }
            }
        }
    }

    *out_count = count;
    return 0;
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


