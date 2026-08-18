#include <shell/shell.h>
#include <drivers/vga.h>
#include <drivers/keyboard.h>
#include <arch/i386/io.h>
#include <arch/i386/timer.h>
#include <kernel/pmm.h>
#include <kernel/kheap.h>
#include <fs/migfs.h>
#include <libc/string.h>
#include <libc/stdlib.h>



volatile int matrix_running = 0;

static char history[HISTORY_MAX][BUFFER_SIZE];
static int history_count = 0;
static int history_index = 0;

static char pending_cmd[BUFFER_SIZE];
static volatile int has_pending_cmd = 0;

void shell_post_command(const char* command) {
    strncpy(pending_cmd, command, BUFFER_SIZE - 1);
    pending_cmd[BUFFER_SIZE - 1] = '\0';
    has_pending_cmd = 1;
}

int shell_has_pending_command(void) {
    return has_pending_cmd;
}

void shell_update(void) {
    if (has_pending_cmd) {
        char cmd[BUFFER_SIZE];
        strncpy(cmd, pending_cmd, BUFFER_SIZE - 1);
        cmd[BUFFER_SIZE - 1] = '\0';
        has_pending_cmd = 0;
        shell_execute(cmd);
    }
}

void shell_history_add(const char* command) {
    if (command[0] == '\0') return;

    if (history_count > 0 && strcmp(history[history_count - 1], command) == 0) {
        history_index = history_count;
        return;
    }

    if (history_count < HISTORY_MAX) {
        strncpy(history[history_count], command, BUFFER_SIZE - 1);
        history[history_count][BUFFER_SIZE - 1] = '\0';
        history_count++;
    } else {
        for (int i = 0; i < HISTORY_MAX - 1; i++) {
            strncpy(history[i], history[i + 1], BUFFER_SIZE);
        }
        strncpy(history[HISTORY_MAX - 1], command, BUFFER_SIZE - 1);
        history[HISTORY_MAX - 1][BUFFER_SIZE - 1] = '\0';
    }
    history_index = history_count;
}

const char* shell_history_up(void) {
    if (history_count == 0) return 0;
    if (history_index > 0) {
        history_index--;
    }
    return history[history_index];
}

const char* shell_history_down(void) {
    if (history_count == 0) return 0;
    if (history_index < history_count - 1) {
        history_index++;
        return history[history_index];
    } else {
        history_index = history_count;
        return "";
    }
}

// Reinicializacao forcada via Triple Fault (Garantido no QEMU e x86 Bare-Metal)
static void reboot_system(void) {
    vga_clear();
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("Reiniciando o migOS...\n");

    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);
    outb(0x64, 0xFE);

    // Carrega IDT com limite 0 e dispara interrupcao (Forca Reset de Hardware)
    __asm__ volatile (
        "cli\n"
        "pushl $0\n"
        "pushl $0\n"
        "lidt (%esp)\n"
        "int $3\n"
    );

    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

static int check_key_pressed(void) {
    if (keyboard_has_key()) {
        keyboard_clear_key();
        return 1;
    }
    if (inb(0x64) & 1) {
        inb(0x60);
        return 1;
    }
    return 0;
}

static int sleep_with_exit_check(unsigned int ms) {
    unsigned int steps = ms / 10;
    if (steps == 0) steps = 1;

    for (unsigned int i = 0; i < steps; i++) {
        if (check_key_pressed()) return 1;
        sleep(10);
    }
    return 0;
}

static void matrix_effect(void) {
    matrix_running = 1;
    keyboard_clear_key();

    vga_clear();

    int drops[VGA_WIDTH];
    for (int x = 0; x < VGA_WIDTH; x++) {
        drops[x] = -(int)(rand() % VGA_HEIGHT);
    }

    // Aguarda 100ms inicial para ignorar o Enter de invocacao do comando
    sleep(100);
    keyboard_clear_key();

    while (matrix_running) {
        if (check_key_pressed()) break;

        for (int x = 0; x < VGA_WIDTH; x++) {
            if (rand() % 3 != 0) continue;

            int y = drops[x];

            if (y >= 0 && y < VGA_HEIGHT) {
                char ch = (rand() % 2 == 0) ? '0' : '1';
                VGA_MEMORY[y * VGA_WIDTH + x] = (unsigned short)ch | ((unsigned short)VGA_COLOR_WHITE << 8);
            }

            if ((y - 1) >= 0 && (y - 1) < VGA_HEIGHT) {
                char ch = (rand() % 2 == 0) ? '0' : '1';
                VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = (unsigned short)ch | ((unsigned short)VGA_COLOR_LIGHT_GREEN << 8);
            }

            if ((y - 2) >= 0 && (y - 2) < VGA_HEIGHT) {
                char ch = (rand() % 2 == 0) ? '0' : '1';
                VGA_MEMORY[(y - 2) * VGA_WIDTH + x] = (unsigned short)ch | ((unsigned short)VGA_COLOR_GREEN << 8);
            }

            if ((y - 8) >= 0 && (y - 8) < VGA_HEIGHT) {
                VGA_MEMORY[(y - 8) * VGA_WIDTH + x] = (unsigned short)' ' | ((unsigned short)VGA_COLOR_BLACK << 8);
            }

            drops[x]++;
            if (drops[x] - 8 >= VGA_HEIGHT) drops[x] = 0;
        }

        if (sleep_with_exit_check(50)) break;
    }

    matrix_running = 0;
    keyboard_clear_key();

    vga_clear();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("Bem-vindo de volta ao mundo real, Neo.\n\n");
}

static void print_meminfo(void) {
    char buf[32];

    size_t total_ram   = pmm_get_total_memory();
    size_t used_ram    = pmm_get_used_memory();
    size_t free_ram    = pmm_get_free_memory();
    size_t total_blks  = pmm_get_total_blocks();
    size_t used_blks   = pmm_get_used_blocks();
    size_t free_blks   = pmm_get_free_blocks();
    size_t blk_size    = pmm_get_block_size();

    size_t heap_total  = kheap_get_total_bytes();
    size_t heap_used   = kheap_get_used_bytes();
    size_t heap_free   = kheap_get_free_bytes();
    size_t heap_allocs = kheap_get_alloc_count();

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("================ MEMORIA FISICA (PMM) ================\n");

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  RAM Total:        ");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    itoa((int)(total_ram / (1024 * 1024)), buf, 10);
    vga_puts(buf);
    vga_puts(" MB (");
    itoa((int)(total_ram / 1024), buf, 10);
    vga_puts(buf);
    vga_puts(" KB)\n");

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  RAM Usada/Res.:   ");
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    itoa((int)(used_ram / 1024), buf, 10);
    vga_puts(buf);
    vga_puts(" KB (");
    itoa((int)used_blks, buf, 10);
    vga_puts(buf);
    vga_puts(" frames)\n");

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  RAM Livre:        ");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    itoa((int)(free_ram / (1024 * 1024)), buf, 10);
    vga_puts(buf);
    vga_puts(" MB (");
    itoa((int)(free_ram / 1024), buf, 10);
    vga_puts(buf);
    vga_puts(" KB)\n");

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  Tamanho do Bloco: ");
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    itoa((int)blk_size, buf, 10);
    vga_puts(buf);
    vga_puts(" bytes (4 KB)\n");

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  Total de Blocos:  ");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    itoa((int)total_blks, buf, 10);
    vga_puts(buf);
    vga_puts(" frames (Livres: ");
    itoa((int)free_blks, buf, 10);
    vga_puts(buf);
    vga_puts(")\n\n");

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("================ HEAP DO KERNEL (KMALLOC) ============\n");

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  Heap Total:       ");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    itoa((int)(heap_total / 1024), buf, 10);
    vga_puts(buf);
    vga_puts(" KB (");
    itoa((int)(heap_total / (1024 * 1024)), buf, 10);
    vga_puts(buf);
    vga_puts(" MB)\n");

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  Heap Usado:       ");
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    itoa((int)heap_used, buf, 10);
    vga_puts(buf);
    vga_puts(" bytes\n");

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  Heap Livre:       ");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    itoa((int)(heap_free / 1024), buf, 10);
    vga_puts(buf);
    vga_puts(" KB\n");

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  Alocacoes Ativas: ");
    vga_set_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    itoa((int)heap_allocs, buf, 10);
    vga_puts(buf);
    vga_puts(" blocos\n");

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("======================================================\n");
}

static void print_memtest(void) {
    char buf[32];
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("[MEMTEST] Iniciando teste de alocacao dinamica...\n");

    // Teste 1: Alocacao via kmalloc
    void* ptr1 = kmalloc(512);
    if (!ptr1) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("[FALHA] kmalloc(512) retornou NULL!\n");
        return;
    }
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("[OK] kmalloc(512) alocado em 0x");
    itoa((int)ptr1, buf, 16);
    vga_puts(buf);
    vga_puts("\n");

    // Preenche memoria com dados de teste
    memset(ptr1, 0xAA, 512);

    // Teste 2: Alocacao de segundo bloco
    void* ptr2 = kmalloc(2048);
    if (!ptr2) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("[FALHA] kmalloc(2048) retornou NULL!\n");
        kfree(ptr1);
        return;
    }
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("[OK] kmalloc(2048) alocado em 0x");
    itoa((int)ptr2, buf, 16);
    vga_puts(buf);
    vga_puts("\n");

    // Teste 3: Alocacao direta de frame de 4KB no PMM
    void* frame = pmm_alloc_block();
    if (!frame) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("[FALHA] pmm_alloc_block() retornou NULL!\n");
        kfree(ptr1);
        kfree(ptr2);
        return;
    }
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("[OK] pmm_alloc_block() frame 4KB em 0x");
    itoa((int)frame, buf, 16);
    vga_puts(buf);
    vga_puts("\n");

    // Libera os recursos alocados
    pmm_free_block(frame);
    kfree(ptr1);
    kfree(ptr2);

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("[OK] Todos os blocos e frames liberados (kfree & pmm_free_block)!\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("[SUCESSO] Teste de Gerenciamento de Memoria concluido com 100% de exito!\n");
}

static void cmd_ls(void) {
    char buf[32];
    size_t count = 0;
    size_t total_bytes = migfs_get_total_used_bytes();

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Arquivo                   Tamanho     Atributos\n");
    vga_puts("--------------------------------------------------\n");

    for (size_t i = 0; i < MIGFS_MAX_FILES; i++) {
        migfs_file_t* f = migfs_get_file_by_index(i);
        if (f) {
            count++;
            // Imprime nome do arquivo com espacamento alinhado
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            vga_puts("  ");
            vga_puts(f->name);

            size_t name_len = strlen(f->name);
            for (size_t p = name_len; p < 24; p++) {
                vga_putc(' ');
            }

            // Imprime tamanho
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            itoa((int)f->size, buf, 10);
            vga_puts(buf);
            vga_puts(" B");

            size_t size_len = strlen(buf) + 2;
            for (size_t p = size_len; p < 12; p++) {
                vga_putc(' ');
            }

            // Imprime atributos
            if (f->flags & MIGFS_FILE_READONLY) {
                vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
                vga_puts("[RO]\n");
            } else {
                vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
                vga_puts("[RW]\n");
            }
        }
    }

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("--------------------------------------------------\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    itoa((int)count, buf, 10);
    vga_puts("Total: ");
    vga_puts(buf);
    vga_puts(" arquivo(s) (");
    itoa((int)total_bytes, buf, 10);
    vga_puts(buf);
    vga_puts(" bytes no RAMDisk)\n");
}

static void cmd_cat(const char* args) {
    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Uso: cat <nome_do_arquivo>\n");
        return;
    }

    migfs_file_t* f = migfs_open(args);
    if (!f) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("cat: arquivo '");
        vga_puts(args);
        vga_puts("' nao encontrado no RAMDisk.\n");
        return;
    }

    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    if (f->data) {
        vga_puts(f->data);
        if (f->size > 0 && f->data[f->size - 1] != '\n') {
            vga_putc('\n');
        }
    }
}

static void cmd_touch(const char* args) {
    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Uso: touch <nome_do_arquivo>\n");
        return;
    }

    if (migfs_exists(args)) {
        vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
        vga_puts("touch: arquivo '");
        vga_puts(args);
        vga_puts("' ja existe no RAMDisk.\n");
        return;
    }

    int ret = migfs_create(args, "", 0, 0);
    if (ret == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Arquivo '");
        vga_puts(args);
        vga_puts("' criado com sucesso no RAMDisk.\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Erro ao criar arquivo (codigo ");
        char buf[16];
        itoa(ret, buf, 10);
        vga_puts(buf);
        vga_puts(").\n");
    }
}

static void cmd_write(const char* args) {
    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Uso: write <arquivo> <texto>\n");
        return;
    }

    char filename[MIGFS_MAX_FILENAME];
    size_t i = 0;
    while (*args != ' ' && *args != '\0' && i < MIGFS_MAX_FILENAME - 1) {
        filename[i++] = *args++;
    }
    filename[i] = '\0';

    while (*args == ' ') args++;

    int ret = migfs_write(filename, args, strlen(args));
    if (ret == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Dados gravados com sucesso em '");
        vga_puts(filename);
        vga_puts("'.\n");
    } else if (ret == -2) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Erro: o arquivo '");
        vga_puts(filename);
        vga_puts("' eh somente leitura [RO]!\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Erro ao gravar dados no arquivo.\n");
    }
}

static void cmd_rm(const char* args) {
    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Uso: rm <nome_do_arquivo>\n");
        return;
    }

    int ret = migfs_delete(args);
    if (ret == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Arquivo '");
        vga_puts(args);
        vga_puts("' removido do RAMDisk.\n");
    } else if (ret == -2) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Erro: o arquivo '");
        vga_puts(args);
        vga_puts("' eh protegido contra exclusao [RO]!\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("rm: arquivo '");
        vga_puts(args);
        vga_puts("' nao encontrado.\n");
    }
}

void shell_init(void) {
    has_pending_cmd = 0;
    pending_cmd[0] = '\0';
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("migOS> ");
}

void shell_execute(const char* command) {
    while (*command == ' ') command++;

    if (command[0] == '\0') {
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_puts("migOS> ");
        return;
    }

    shell_history_add(command);

    if (strcmp(command, "help") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        vga_puts("Comandos disponiveis:\n");
        vga_puts("  help                - Exibe esta lista de comandos\n");
        vga_puts("  clear               - Limpa a tela do terminal\n");
        vga_puts("  ls                  - Lista arquivos no RAMDisk (MIGFS)\n");
        vga_puts("  cat <arquivo>       - Exibe o conteudo de um arquivo\n");
        vga_puts("  touch <arquivo>     - Cria um novo arquivo vazio\n");
        vga_puts("  write <arq> <texto> - Escreve texto em um arquivo\n");
        vga_puts("  rm <arquivo>        - Remove um arquivo do RAMDisk\n");
        vga_puts("  meminfo             - Exibe status da memoria (PMM e Heap)\n");
        vga_puts("  memtest             - Executa teste de alocacao dinamica\n");
        vga_puts("  uptime              - Exibe o tempo de atividade do sistema\n");
        vga_puts("  matrix              - Inicia a chuva de codigos Matrix\n");
        vga_puts("  version             - Exibe a versao atual do kernel\n");
        vga_puts("  about               - Informacoes sobre o autor e o sistema\n");
        vga_puts("  panic               - Dispara Kernel Panic de teste\n");
        vga_puts("  reboot              - Reinicia a maquina virtual\n");
    } else if (strcmp(command, "clear") == 0) {
        vga_clear();
    } else if (strcmp(command, "ls") == 0) {
        cmd_ls();
    } else if (strncmp(command, "cat", 3) == 0 && (command[3] == ' ' || command[3] == '\0')) {
        cmd_cat(command + 3);
    } else if (strncmp(command, "touch", 5) == 0 && (command[5] == ' ' || command[5] == '\0')) {
        cmd_touch(command + 5);
    } else if (strncmp(command, "write", 5) == 0 && (command[5] == ' ' || command[5] == '\0')) {
        cmd_write(command + 5);
    } else if (strncmp(command, "rm", 2) == 0 && (command[2] == ' ' || command[2] == '\0')) {
        cmd_rm(command + 2);
    } else if (strcmp(command, "meminfo") == 0) {
        print_meminfo();
    } else if (strcmp(command, "memtest") == 0) {
        print_memtest();
    } else if (strcmp(command, "uptime") == 0) {
        char buf[32];
        unsigned int sec = get_uptime();
        unsigned int ticks = timer_get_ticks();
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("migOS Uptime: ");
        itoa((int)sec, buf, 10);
        vga_puts(buf);
        vga_puts(" segundos (");
        itoa((int)ticks, buf, 10);
        vga_puts(buf);
        vga_puts(" ticks do PIT)\n");
    } else if (strcmp(command, "matrix") == 0) {
        matrix_effect();
    } else if (strcmp(command, "version") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("migOS Kernel v0.5 (32-bit Protected Mode)\n");
    } else if (strcmp(command, "about") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
        vga_puts("migOS - Sistema Operacional Desenvolvido por Miguel\n");
        vga_puts("Arquitetura: x86 (IA-32) Bare-Metal\n");
    } else if (strcmp(command, "panic") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Disparando excecao de CPU (ISR 0)...\n");
        __asm__ volatile ("int $0");
    } else if (strcmp(command, "reboot") == 0) {
        reboot_system();
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Comando desconhecido: '");
        vga_puts(command);
        vga_puts("'\n");
    }

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("migOS> ");
}


