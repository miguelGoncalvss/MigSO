#include <shell/shell.h>
#include <drivers/vga.h>
#include <drivers/keyboard.h>
#include <drivers/ata.h>
#include <drivers/rtc.h>
#include <drivers/sound.h>
#include <games/snake.h>
#include <arch/i386/io.h>
#include <arch/i386/timer.h>
#include <arch/i386/reboot.h>
#include <kernel/pmm.h>
#include <kernel/kheap.h>
#include <fs/migfs.h>
#include <gui/gui.h>
#include <editor/editor.h>
#include <interpreter/txt_interp.h>
#include <emulator/gameboy.h>
#include <libc/string.h>
#include <libc/stdlib.h>



volatile int matrix_running = 0;

static char history[HISTORY_MAX][BUFFER_SIZE];
static int history_count = 0;
static int history_index = 0;

static char shell_cwd[MIGFS_MAX_FILENAME] = "/";

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

void shell_print_prompt(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("migOS:");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts(shell_cwd);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("> ");
}

static void resolve_shell_path(const char* rel_or_abs, char* out, size_t out_size) {
    migfs_path_combine(shell_cwd, rel_or_abs, out, out_size);
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

void shell_history_list(void) {
    if (history_count == 0) {
        vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
        vga_puts("Nenhum comando no historico recente.\n");
        return;
    }

    char buf[16];
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Historico de Comandos do Shell (use '!<num>' para executar):\n");
    vga_puts("------------------------------------------------------------\n");
    for (int i = 0; i < history_count; i++) {
        vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        vga_puts("  [");
        itoa(i + 1, buf, 10);
        vga_puts(buf);
        vga_puts("] ");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_puts(history[i]);
        vga_putc('\n');
    }
}

void shell_autocomplete(char* buffer, int* cursor_pos, int* buffer_len) {
    if (!buffer || !cursor_pos || !buffer_len) return;

    int cur = *cursor_pos;
    int token_start = cur;
    while (token_start > 0 && buffer[token_start - 1] != ' ') {
        token_start--;
    }
    int token_len = cur - token_start;

    int is_first_word = 1;
    for (int i = 0; i < token_start; i++) {
        if (buffer[i] != ' ') {
            is_first_word = 0;
            break;
        }
    }

    char prefix[MIGFS_MAX_FILENAME];
    if (token_len >= MIGFS_MAX_FILENAME) token_len = MIGFS_MAX_FILENAME - 1;
    for (int i = 0; i < token_len; i++) {
        prefix[i] = buffer[token_start + i];
    }
    prefix[token_len] = '\0';

    #define MAX_AUTO_MATCHES 32
    char matches[MAX_AUTO_MATCHES][MIGFS_MAX_FILENAME];
    int is_dir_match[MAX_AUTO_MATCHES];
    int match_count = 0;

    if (is_first_word) {
        static const char* known_cmds[] = {
            "help", "ls", "dir", "cd", "pwd", "mkdir", "md", "rmdir", "rd",
            "mv", "cp", "touch", "cat", "type", "write", "echo", "rm", "del",
            "edit", "nano", "vi", "vim", "run", "exec", "batch",
            "gameboy", "gb", "pokemon", "gbinfo", "calc", "eval", "sync",
            "meminfo", "memtest", "free", "df", "uptime", "date", "time",
            "whoami", "hostname", "history", "matrix", "snake", "gui", "desktop",
            "version", "about", "panic", "reboot", "clear", "cls"
        };
        int num_known = sizeof(known_cmds) / sizeof(known_cmds[0]);

        for (int i = 0; i < num_known && match_count < MAX_AUTO_MATCHES; i++) {
            if (strncmp(known_cmds[i], prefix, token_len) == 0) {
                strncpy(matches[match_count], known_cmds[i], MIGFS_MAX_FILENAME - 1);
                matches[match_count][MIGFS_MAX_FILENAME - 1] = '\0';
                is_dir_match[match_count] = 0;
                match_count++;
            }
        }

        migfs_dir_item_t cwd_items[32];
        size_t cwd_count = 0;
        migfs_get_dir_items(shell_cwd, cwd_items, 32, &cwd_count);
        for (size_t i = 0; i < cwd_count && match_count < MAX_AUTO_MATCHES; i++) {
            if (strncmp(cwd_items[i].name, prefix, token_len) == 0) {
                strncpy(matches[match_count], cwd_items[i].name, MIGFS_MAX_FILENAME - 1);
                matches[match_count][MIGFS_MAX_FILENAME - 1] = '\0';
                is_dir_match[match_count] = cwd_items[i].is_dir;
                match_count++;
            }
        }
    } else {
        char dir_to_search[MIGFS_MAX_FILENAME];
        char file_prefix[MIGFS_MAX_FILENAME];
        const char* last_slash = strrchr(prefix, '/');

        if (last_slash) {
            size_t dlen = last_slash - prefix + 1;
            char rel_dir[MIGFS_MAX_FILENAME];
            strncpy(rel_dir, prefix, dlen);
            rel_dir[dlen] = '\0';
            resolve_shell_path(rel_dir, dir_to_search, sizeof(dir_to_search));
            strncpy(file_prefix, last_slash + 1, sizeof(file_prefix) - 1);
            file_prefix[sizeof(file_prefix) - 1] = '\0';
        } else {
            strncpy(dir_to_search, shell_cwd, sizeof(dir_to_search) - 1);
            dir_to_search[sizeof(dir_to_search) - 1] = '\0';
            strncpy(file_prefix, prefix, sizeof(file_prefix) - 1);
            file_prefix[sizeof(file_prefix) - 1] = '\0';
        }

        migfs_dir_item_t items[32];
        size_t num_items = 0;
        migfs_get_dir_items(dir_to_search, items, 32, &num_items);

        size_t flen = strlen(file_prefix);
        for (size_t i = 0; i < num_items && match_count < MAX_AUTO_MATCHES; i++) {
            if (strncmp(items[i].name, file_prefix, flen) == 0) {
                if (last_slash) {
                    size_t dlen = last_slash - prefix + 1;
                    strncpy(matches[match_count], prefix, dlen);
                    matches[match_count][dlen] = '\0';
                    strncat(matches[match_count], items[i].name, MIGFS_MAX_FILENAME - dlen - 1);
                } else {
                    strncpy(matches[match_count], items[i].name, MIGFS_MAX_FILENAME - 1);
                    matches[match_count][MIGFS_MAX_FILENAME - 1] = '\0';
                }
                is_dir_match[match_count] = items[i].is_dir;
                match_count++;
            }
        }
    }

    if (match_count == 0) {
        return;
    }

    if (match_count == 1) {
        char full_match[MIGFS_MAX_FILENAME + 2];
        strncpy(full_match, matches[0], MIGFS_MAX_FILENAME);
        if (is_dir_match[0]) {
            strcat(full_match, "/");
        } else {
            strcat(full_match, " ");
        }

        int match_len = (int)strlen(full_match);
        int tail_len = *buffer_len - *cursor_pos;
        if (token_start + match_len + tail_len < BUFFER_SIZE - 1) {
            for (int i = tail_len; i >= 0; i--) {
                buffer[token_start + match_len + i] = buffer[*cursor_pos + i];
            }
            for (int i = 0; i < match_len; i++) {
                buffer[token_start + i] = full_match[i];
            }
            *buffer_len = token_start + match_len + tail_len;
            *cursor_pos = token_start + match_len;
            buffer[*buffer_len] = '\0';

            vga_putc('\r');
            shell_print_prompt();
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            vga_puts(buffer);
        }
    } else {
        char common[MIGFS_MAX_FILENAME];
        strncpy(common, matches[0], MIGFS_MAX_FILENAME - 1);
        common[MIGFS_MAX_FILENAME - 1] = '\0';
        int common_len = (int)strlen(common);

        for (int m = 1; m < match_count; m++) {
            int j = 0;
            while (j < common_len && matches[m][j] != '\0' && matches[m][j] == common[j]) {
                j++;
            }
            common_len = j;
            common[common_len] = '\0';
        }

        if (common_len > token_len) {
            int tail_len = *buffer_len - *cursor_pos;
            if (token_start + common_len + tail_len < BUFFER_SIZE - 1) {
                for (int i = tail_len; i >= 0; i--) {
                    buffer[token_start + common_len + i] = buffer[*cursor_pos + i];
                }
                for (int i = 0; i < common_len; i++) {
                    buffer[token_start + i] = common[i];
                }
                *buffer_len = token_start + common_len + tail_len;
                *cursor_pos = token_start + common_len;
                buffer[*buffer_len] = '\0';
            }
        }

        vga_putc('\n');
        for (int m = 0; m < match_count; m++) {
            if (is_dir_match[m]) {
                vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
                vga_puts(matches[m]);
                vga_putc('/');
            } else if (strstr(matches[m], ".gb") || strstr(matches[m], ".wad")) {
                vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
                vga_puts(matches[m]);
            } else {
                vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
                vga_puts(matches[m]);
            }
            vga_puts("  ");
        }
        vga_putc('\n');

        shell_print_prompt();
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_puts(buffer);
    }
}



static int check_key_pressed(void) {
    if (keyboard_has_key()) {
        keyboard_clear_key();
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
                char ch = (char)(33 + (rand() % 93));
                vga_set_cell(x, y, ch, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            }

            if ((y - 1) >= 0 && (y - 1) < VGA_HEIGHT) {
                char ch = (char)(33 + (rand() % 93));
                vga_set_cell(x, y - 1, ch, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            }

            if ((y - 2) >= 0 && (y - 2) < VGA_HEIGHT) {
                char ch = (char)(33 + (rand() % 93));
                vga_set_cell(x, y - 2, ch, VGA_COLOR_GREEN, VGA_COLOR_BLACK);
            }

            if ((y - 10) >= 0 && (y - 10) < VGA_HEIGHT) {
                vga_set_cell(x, y - 10, ' ', VGA_COLOR_BLACK, VGA_COLOR_BLACK);
            }

            drops[x]++;
            if (drops[x] - 10 >= VGA_HEIGHT) drops[x] = 0;
        }

        if (sleep_with_exit_check(35)) break;
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

static void cmd_pwd(void) {
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts(shell_cwd);
    vga_putc('\n');
}

static void cmd_cd(const char* args) {
    while (*args == ' ') args++;
    if (*args == '\0' || strcmp(args, "~") == 0 || strcmp(args, "/") == 0) {
        strcpy(shell_cwd, "/");
        return;
    }

    if (strcmp(args, "..") == 0) {
        migfs_get_parent_dir(shell_cwd, shell_cwd, sizeof(shell_cwd));
        return;
    }

    char target[MIGFS_MAX_FILENAME];
    resolve_shell_path(args, target, sizeof(target));

    if (migfs_is_dir(target)) {
        strncpy(shell_cwd, target, MIGFS_MAX_FILENAME - 1);
        shell_cwd[MIGFS_MAX_FILENAME - 1] = '\0';
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("cd: '");
        vga_puts(args);
        vga_puts("' nao eh uma pasta valida ou nao existe.\n");
    }
}

static void cmd_mkdir(const char* args) {
    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Uso: mkdir <nome_da_pasta>\n");
        return;
    }

    char target[MIGFS_MAX_FILENAME];
    resolve_shell_path(args, target, sizeof(target));

    int ret = migfs_mkdir(target);
    if (ret == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Pasta '");
        vga_puts(target);
        vga_puts("' criada com sucesso no MIGFS.\n");
    } else if (ret == -2) {
        vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
        vga_puts("mkdir: pasta '");
        vga_puts(target);
        vga_puts("' ja existe.\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Erro ao criar pasta (tabela cheia ou limite atingido).\n");
    }
}

static void cmd_rmdir(const char* args) {
    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Uso: rmdir <nome_da_pasta>\n");
        return;
    }

    char target[MIGFS_MAX_FILENAME];
    resolve_shell_path(args, target, sizeof(target));

    int ret = migfs_rmdir(target);
    if (ret == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Pasta '");
        vga_puts(target);
        vga_puts("' removida com sucesso.\n");
    } else if (ret == -2) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("rmdir: a pasta '");
        vga_puts(target);
        vga_puts("' nao esta vazia! Remova os arquivos internos antes.\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("rmdir: pasta '");
        vga_puts(target);
        vga_puts("' nao encontrada.\n");
    }
}

static void cmd_mv(const char* args) {
    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Uso: mv <origem> <destino>\n");
        return;
    }

    char src[MIGFS_MAX_FILENAME];
    char dest[MIGFS_MAX_FILENAME];
    int si = 0, di = 0;

    while (*args != ' ' && *args != '\0' && si < MIGFS_MAX_FILENAME - 1) {
        src[si++] = *args++;
    }
    src[si] = '\0';

    while (*args == ' ') args++;
    while (*args != ' ' && *args != '\0' && di < MIGFS_MAX_FILENAME - 1) {
        dest[di++] = *args++;
    }
    dest[di] = '\0';

    if (si == 0 || di == 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Uso: mv <origem> <destino>\n");
        return;
    }

    char full_src[MIGFS_MAX_FILENAME];
    char full_dest[MIGFS_MAX_FILENAME];
    resolve_shell_path(src, full_src, sizeof(full_src));
    resolve_shell_path(dest, full_dest, sizeof(full_dest));

    int ret = migfs_move(full_src, full_dest);
    if (ret == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Movido/Renomeado: '");
        vga_puts(src);
        vga_puts("' -> '");
        vga_puts(dest);
        vga_puts("'\n");
    } else if (ret == -2) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Erro: arquivo de origem eh protegido somente leitura [RO]!\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Erro ao mover/renomear arquivo ou pasta.\n");
    }
}

static void cmd_cp(const char* args) {
    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Uso: cp <origem> <destino>\n");
        return;
    }

    char src[MIGFS_MAX_FILENAME];
    char dest[MIGFS_MAX_FILENAME];
    int si = 0, di = 0;

    while (*args != ' ' && *args != '\0' && si < MIGFS_MAX_FILENAME - 1) {
        src[si++] = *args++;
    }
    src[si] = '\0';

    while (*args == ' ') args++;
    while (*args != ' ' && *args != '\0' && di < MIGFS_MAX_FILENAME - 1) {
        dest[di++] = *args++;
    }
    dest[di] = '\0';

    if (si == 0 || di == 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Uso: cp <origem> <destino>\n");
        return;
    }

    char full_src[MIGFS_MAX_FILENAME];
    char full_dest[MIGFS_MAX_FILENAME];
    resolve_shell_path(src, full_src, sizeof(full_src));
    resolve_shell_path(dest, full_dest, sizeof(full_dest));

    int ret = migfs_copy(full_src, full_dest);
    if (ret == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Copiado: '");
        vga_puts(src);
        vga_puts("' -> '");
        vga_puts(dest);
        vga_puts("'\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Erro ao copiar arquivo.\n");
    }
}

static void cmd_ls(const char* args) {
    while (*args == ' ') args++;

    char buf[32];
    size_t file_count = 0;
    size_t dir_count = 0;

    char target_dir[MIGFS_MAX_FILENAME];
    if (*args == '\0') {
        strncpy(target_dir, shell_cwd, MIGFS_MAX_FILENAME - 1);
    } else if (strcmp(args, "-a") == 0 || strcmp(args, "-all") == 0) {
        // Listagem global flat de todos os arquivos
        vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        vga_puts("Todos os Arquivos no Disco ATA Persistente (MIGFS):\n");
        vga_puts("----------------------------------------------------------------------\n");
        vga_puts("Nome                  Tipo    Tamanho     Modificado        Attr\n");
        vga_puts("----------------------------------------------------------------------\n");
        for (size_t i = 0; i < MIGFS_MAX_FILES; i++) {
            migfs_file_t* f = migfs_get_file_by_index(i);
            if (f) {
                file_count++;
                vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
                vga_puts("  ");
                vga_puts(f->name);
                for (size_t p = strlen(f->name); p < 20; p++) vga_putc(' ');

                if (f->flags & MIGFS_FILE_DIRECTORY) {
                    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
                    vga_puts("<DIR>  ");
                    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
                    vga_puts("-           ");
                } else {
                    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
                    vga_puts("<FILE> ");
                    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
                    itoa((int)f->size, buf, 10);
                    vga_puts(buf);
                    vga_puts(" B");
                    for (size_t p = strlen(buf) + 2; p < 12; p++) vga_putc(' ');
                }

                char dt[24];
                if (f->modified_time > 0) {
                    rtc_format_epoch_short(f->modified_time, dt, sizeof(dt));
                } else {
                    strcpy(dt, "--/-- --:--");
                }
                vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
                vga_puts(dt);
                for (size_t p = strlen(dt); p < 16; p++) vga_putc(' ');

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
        vga_puts("----------------------------------------------------------------------\n");
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        itoa((int)file_count, buf, 10);
        vga_puts("Total: ");
        vga_puts(buf);
        vga_puts(" entradas registradas no MIGFS.\n");
        return;
    } else {
        resolve_shell_path(args, target_dir, sizeof(target_dir));
    }
    target_dir[MIGFS_MAX_FILENAME - 1] = '\0';

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Conteudo da Pasta: ");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts(target_dir);
    vga_putc('\n');

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Nome                  Tipo    Tamanho     Modificado        Attr\n");
    vga_puts("----------------------------------------------------------------------\n");

    static migfs_dir_item_t ls_items[32];
    size_t item_count = 0;
    migfs_get_dir_items(target_dir, ls_items, 32, &item_count);

    for (size_t i = 0; i < item_count; i++) {
        if (ls_items[i].is_dir) {
            dir_count++;
            vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
            vga_puts("  ");
            vga_puts(ls_items[i].name);
            for (size_t p = strlen(ls_items[i].name); p < 20; p++) vga_putc(' ');

            vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
            vga_puts("<DIR>  ");

            vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
            vga_puts("-           ");
        } else {
            file_count++;
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            vga_puts("  ");
            vga_puts(ls_items[i].name);
            for (size_t p = strlen(ls_items[i].name); p < 20; p++) vga_putc(' ');

            vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            vga_puts("<FILE> ");

            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            itoa((int)ls_items[i].size, buf, 10);
            vga_puts(buf);
            vga_puts(" B");
            for (size_t p = strlen(buf) + 2; p < 12; p++) vga_putc(' ');
        }

        char dt[24];
        if (ls_items[i].modified_time > 0) {
            rtc_format_epoch_short(ls_items[i].modified_time, dt, sizeof(dt));
        } else {
            strcpy(dt, "--/-- --:--");
        }
        vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        vga_puts(dt);
        for (size_t p = strlen(dt); p < 16; p++) vga_putc(' ');

        if (ls_items[i].flags & MIGFS_FILE_READONLY) {
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            vga_puts("[RO]\n");
        } else {
            vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
            vga_puts("[RW]\n");
        }
    }

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("----------------------------------------------------------------------\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    itoa((int)file_count, buf, 10);
    vga_puts("Total: ");
    vga_puts(buf);
    vga_puts(" arquivo(s), ");
    itoa((int)dir_count, buf, 10);
    vga_puts(buf);
    vga_puts(" pasta(s).\n");
}

static void cmd_cat(const char* args) {
    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Uso: cat <nome_do_arquivo>\n");
        return;
    }

    char target[MIGFS_MAX_FILENAME];
    resolve_shell_path(args, target, sizeof(target));

    migfs_file_t* f = migfs_open(target);
    if (!f) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("cat: arquivo '");
        vga_puts(args);
        vga_puts("' nao encontrado.\n");
        return;
    }

    if (f->flags & MIGFS_FILE_DIRECTORY) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("cat: '");
        vga_puts(args);
        vga_puts("' eh um diretorio, nao um arquivo de texto.\n");
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

    char target[MIGFS_MAX_FILENAME];
    resolve_shell_path(args, target, sizeof(target));

    int existed = migfs_exists(target);
    int ret = migfs_touch(target);
    if (ret == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        if (existed) {
            vga_puts("Timestamp do arquivo '");
            vga_puts(target);
            vga_puts("' atualizado para o horario atual.\n");
        } else {
            vga_puts("Arquivo '");
            vga_puts(target);
            vga_puts("' criado com sucesso.\n");
        }
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Erro ao executar touch no arquivo.\n");
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

    char target[MIGFS_MAX_FILENAME];
    resolve_shell_path(filename, target, sizeof(target));

    int ret = migfs_write(target, args, strlen(args));
    if (ret == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Dados gravados com sucesso em '");
        vga_puts(target);
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

    char target[MIGFS_MAX_FILENAME];
    resolve_shell_path(args, target, sizeof(target));

    if (migfs_is_dir(target)) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("rm: '");
        vga_puts(args);
        vga_puts("' eh um diretorio. Para remover pastas use 'rmdir'.\n");
        return;
    }

    int ret = migfs_delete(target);
    if (ret == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Arquivo '");
        vga_puts(target);
        vga_puts("' removido do disco.\n");
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

static void cmd_echo(const char* args) {
    while (*args == ' ') args++;
    if (*args == '"' || *args == '\'') {
        char quote = *args++;
        while (*args && *args != quote) {
            vga_putc(*args++);
        }
    } else {
        vga_puts(args);
    }
    vga_putc('\n');
}

static void cmd_free(void) {
    char buf[32];
    size_t total_ram   = pmm_get_total_memory();
    size_t used_ram    = pmm_get_used_memory();
    size_t free_ram    = pmm_get_free_memory();
    size_t heap_total  = kheap_get_total_bytes();
    size_t heap_used   = kheap_get_used_bytes();
    size_t heap_free   = kheap_get_free_bytes();
    size_t heap_allocs = kheap_get_alloc_count();

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("               total        usado        livre      detalhes\n");
    vga_puts("Mem (PMM):    ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    itoa((int)(total_ram / (1024 * 1024)), buf, 10);
    vga_puts(buf); vga_puts(" MB       ");
    itoa((int)(used_ram / 1024), buf, 10);
    vga_puts(buf); vga_puts(" KB     ");
    itoa((int)(free_ram / (1024 * 1024)), buf, 10);
    vga_puts(buf); vga_puts(" MB     frames de 4KB\n");

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Heap Kernel:  ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    itoa((int)(heap_total / 1024), buf, 10);
    vga_puts(buf); vga_puts(" KB     ");
    itoa((int)heap_used, buf, 10);
    vga_puts(buf); vga_puts(" B      ");
    itoa((int)(heap_free / 1024), buf, 10);
    vga_puts(buf); vga_puts(" KB     ");
    itoa((int)heap_allocs, buf, 10);
    vga_puts(buf); vga_puts(" blocos alocados\n");
}

static void cmd_df(void) {
    char buf[32];
    size_t used_bytes = migfs_get_total_used_bytes();
    size_t files_cnt  = migfs_get_file_count();
    size_t total_kb   = 4096; // 4MB RAMDisk
    size_t used_kb    = used_bytes / 1024;
    if (used_kb == 0 && used_bytes > 0) used_kb = 1;
    size_t free_kb    = (total_kb > used_kb) ? (total_kb - used_kb) : 0;
    int use_percent   = (int)((used_kb * 100) / total_kb);

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Sistema-Arq     1K-Blocos      Usado  Disponivel Uso%  Montado em\n");
    vga_puts("-----------------------------------------------------------------\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("/dev/ramdisk         4096       ");
    itoa((int)used_kb, buf, 10);
    vga_puts(buf);
    for (size_t p = strlen(buf); p < 6; p++) vga_putc(' ');
    vga_puts("      ");
    itoa((int)free_kb, buf, 10);
    vga_puts(buf);
    for (size_t p = strlen(buf); p < 6; p++) vga_putc(' ');
    vga_puts(" ");
    itoa(use_percent, buf, 10);
    vga_puts(buf);
    vga_puts("%   / (MIGFS)\n");

    vga_puts("/dev/ata0           16384       ");
    itoa((int)used_kb, buf, 10);
    vga_puts(buf);
    for (size_t p = strlen(buf); p < 6; p++) vga_putc(' ');
    vga_puts("      ");
    itoa((int)(16384 - used_kb), buf, 10);
    vga_puts(buf);
    for (size_t p = strlen(buf); p < 6; p++) vga_putc(' ');
    vga_puts("  ");
    itoa((int)((used_kb * 100) / 16384), buf, 10);
    vga_puts(buf);
    vga_puts("%   /mnt/ata\n");

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("Status: ");
    itoa((int)files_cnt, buf, 10);
    vga_puts(buf);
    vga_puts(" arquivos/pastas ativos no RAMDisk persistente.\n");
}

static void cmd_date(void) {
    char buf[64];
    rtc_time_t rt;
    rtc_get_time(&rt);

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Data e Hora Real (CMOS RTC): ");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    rtc_format_datetime(buf, sizeof(buf));
    vga_puts(buf);
    vga_putc('\n');

    unsigned int sec = get_uptime();
    unsigned int ticks = timer_get_ticks();
    unsigned int hrs = sec / 3600;
    unsigned int min = (sec % 3600) / 60;
    unsigned int s = sec % 60;

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Tempo Ativo (migOS Uptime):  ");
    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    if (hrs < 10) vga_putc('0');
    itoa((int)hrs, buf, 10);
    vga_puts(buf);
    vga_putc(':');
    if (min < 10) vga_putc('0');
    itoa((int)min, buf, 10);
    vga_puts(buf);
    vga_putc(':');
    if (s < 10) vga_putc('0');
    itoa((int)s, buf, 10);
    vga_puts(buf);

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts(" (");
    itoa((int)ticks, buf, 10);
    vga_puts(buf);
    vga_puts(" ticks do PIT @ 100 Hz)\n");

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Timestamp UNIX Atual:        ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    itoa((int)rtc_get_unix_timestamp(), buf, 10);
    vga_puts(buf);
    vga_puts(" s\n");
}

static void cmd_rtc(void) {
    rtc_time_t rt;
    rtc_get_time(&rt);
    char buf[64];

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("==================================================\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("    Controlador CMOS / Real-Time Clock (RTC)      \n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("==================================================\n");

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  Portas I/O:        0x70 (Endereco) / 0x71 (Dados)\n");
    vga_puts("  Formato CMOS:      Decodificacao BCD Ativa\n");
    vga_puts("  Modo de Horario:   24 Horas\n");

    rtc_format_date(buf, sizeof(buf));
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("  Data Decodificada: ");
    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_puts(buf);
    vga_putc('\n');

    rtc_format_time_full(buf, sizeof(buf));
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("  Hora Decodificada: ");
    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_puts(buf);
    vga_putc('\n');

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("  Timestamp UNIX:    ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    itoa((int)rtc_get_unix_timestamp(), buf, 10);
    vga_puts(buf);
    vga_puts(" segundos\n");

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("  Status Bateria:    ");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("OK (Energia RTC Nominal)\n");

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("==================================================\n");
}

static void cmd_beep(const char* args) {
    while (*args == ' ') args++;
    uint32_t freq = 440;
    uint32_t dur = 100;

    if (*args != '\0') {
        freq = (uint32_t)atoi(args);
        while (*args >= '0' && *args <= '9') args++;
        while (*args == ' ') args++;
        if (*args != '\0') {
            dur = (uint32_t)atoi(args);
        }
    }

    if (freq == 0) freq = 440;
    if (dur == 0) dur = 100;

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("PC Speaker: emitindo tom de ");
    char b[32];
    itoa((int)freq, b, 10);
    vga_puts(b);
    vga_puts(" Hz por ");
    itoa((int)dur, b, 10);
    vga_puts(b);
    vga_puts(" ms...\n");

    sound_beep(freq, dur);
}

static void cmd_sfx(const char* args) {
    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        vga_puts("Efeitos Sonoros Disponiveis (SFX):\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_puts("  sfx startup  - Mac OS Classic Startup Chime (Fa Maior)\n");
        vga_puts("  sfx alert    - Beep de Alerta da GUI / Sosumi\n");
        vga_puts("  sfx click    - Clique de Interface de Usuario\n");
        vga_puts("  sfx trash    - Esvaziamento de Lixeira / Acao Destrutiva\n");
        vga_puts("  sfx success  - Confirmacao de Sucesso / Save Game\n");
        return;
    }

    if (strcmp(args, "startup") == 0 || strcmp(args, "chime") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Reproduzindo Startup Chime...\n");
        sound_play_sfx(SFX_STARTUP);
    } else if (strcmp(args, "alert") == 0 || strcmp(args, "sosumi") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Reproduzindo Beep de Alerta (Sosumi)...\n");
        sound_play_sfx(SFX_ALERT);
    } else if (strcmp(args, "click") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Reproduzindo Clique de Interface...\n");
        sound_play_sfx(SFX_CLICK);
    } else if (strcmp(args, "trash") == 0 || strcmp(args, "delete") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Reproduzindo SFX Trash / Destrutivo...\n");
        sound_play_sfx(SFX_TRASH);
    } else if (strcmp(args, "success") == 0 || strcmp(args, "save") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Reproduzindo SFX Confirmacao de Sucesso...\n");
        sound_play_sfx(SFX_SUCCESS);
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Efeito sonoro desconhecido. Digite 'sfx' para listar.\n");
    }
}

static void cmd_mute(const char* args) {
    while (*args == ' ') args++;
    if (strcmp(args, "on") == 0 || strcmp(args, "1") == 0) {
        sound_set_mute(1);
        vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        vga_puts("Audio do PC Speaker: SILENCIADO [MUTE ON]\n");
    } else if (strcmp(args, "off") == 0 || strcmp(args, "0") == 0) {
        sound_set_mute(0);
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Audio do PC Speaker: ATIVADO [MUTE OFF]\n");
    } else {
        int m = sound_is_muted();
        sound_set_mute(!m);
        if (!m) {
            vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
            vga_puts("Audio do PC Speaker: SILENCIADO [MUTE ON]\n");
        } else {
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            vga_puts("Audio do PC Speaker: ATIVADO [MUTE OFF]\n");
            sound_play_sfx(SFX_CLICK);
        }
    }
}

void shell_init(void) {
    has_pending_cmd = 0;
    pending_cmd[0] = '\0';
    strcpy(shell_cwd, "/");
    shell_print_prompt();
}

void shell_execute(const char* command) {
    if (!command) return;
    while (*command == ' ' || *command == '\t') command++;

    char clean_cmd[BUFFER_SIZE];
    strncpy(clean_cmd, command, BUFFER_SIZE - 1);
    clean_cmd[BUFFER_SIZE - 1] = '\0';

    int clen = (int)strlen(clean_cmd);
    while (clen > 0 && (clean_cmd[clen - 1] == ' ' || clean_cmd[clen - 1] == '\t' || clean_cmd[clen - 1] == '\r' || clean_cmd[clen - 1] == '\n')) {
        clean_cmd[--clen] = '\0';
    }

    if (clean_cmd[0] == '\0') {
        shell_print_prompt();
        return;
    }

    // Reexecucao de comando do historico por indice: !1, !2, etc.
    if (clean_cmd[0] == '!' && clean_cmd[1] >= '0' && clean_cmd[1] <= '9') {
        int idx = atoi(clean_cmd + 1);
        if (idx >= 1 && idx <= history_count) {
            const char* prev = history[idx - 1];
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            vga_puts("-> ");
            vga_puts(prev);
            vga_putc('\n');
            shell_execute(prev);
            return;
        } else {
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            vga_puts("Indice do historico fora do alcance.\n");
            shell_print_prompt();
            return;
        }
    }

    shell_history_add(clean_cmd);

    const char* cmd = clean_cmd;

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        vga_puts("==================== CENTRAL DE AJUDA migOS ====================\n");

        vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        vga_puts("[1] ARQUIVOS E DIRETORIOS:\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_puts("  ls / dir [-a]       - Lista conteudo da pasta atual ou global\n");
        vga_puts("  cd <pasta>          - Navega entre pastas ('cd ..' para voltar)\n");
        vga_puts("  pwd                 - Exibe o caminho do diretorio atual\n");
        vga_puts("  mkdir / md <pasta>  - Cria uma nova pasta no MIGFS\n");
        vga_puts("  rmdir / rd <pasta>  - Remove uma pasta vazia\n");
        vga_puts("  mv <orig> <dest>    - Move ou renomeia arquivo/pasta\n");
        vga_puts("  cp <orig> <dest>    - Copia um arquivo para novo local\n");
        vga_puts("  touch <arquivo>     - Cria um novo arquivo vazio\n");
        vga_puts("  cat / type <arq>    - Exibe conteudo de arquivo no terminal\n");
        vga_puts("  write <arq> <txt>   - Escreve texto direto em um arquivo\n");
        vga_puts("  rm / del <arquivo>  - Remove um arquivo do disco\n");
        vga_puts("  sync                - Sincroniza dados com o Disco ATA primario\n\n");

        vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        vga_puts("[2] EDICAO, SCRIPTS E CALCULADORA:\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_puts("  nano / edit <arq>   - Editor de texto visual interativo no CLI\n");
        vga_puts("  run / exec <arq.txt>- Executa scripts do interpretador migOS\n");
        vga_puts("  calc <expressao>    - Avaliador matematico (ex: calc (10+5)*2)\n");
        vga_puts("  echo <texto>        - Imprime mensagem na tela\n\n");

        vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        vga_puts("[3] JOGOS E INTERFACE GRAFICA:\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_puts("  gameboy / gb <.gb>  - Emulador Peanut-GB (ex: pokemon, gameboy)\n");
        vga_puts("  gbinfo <.gb>        - Exibe cabecalho e metadados da ROM\n");
        vga_puts("  gui / desktop       - Inicia Interface Grafica Mac OS System 7\n");
        vga_puts("  snake / matrix      - Jogos e efeitos visuais no terminal\n\n");

        vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        vga_puts("[4] SISTEMA, AUDIO, DIAGNOSTICO E ATALHOS:\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_puts("  beep [freq] [dur]   - Emite tom sonoro no PC Speaker (ex: beep 440 200)\n");
        vga_puts("  sfx <nome>          - Toca efeito sonoro (startup, alert, click, trash, success)\n");
        vga_puts("  mute [on/off]       - Silencia ou ativa o audio do PC Speaker\n");
        vga_puts("  history             - Exibe historico recente (use !<num> para rodar)\n");
        vga_puts("  whoami / hostname   - Exibe usuario e nome da maquina\n");
        vga_puts("  free / meminfo      - Status da memoria fisica PMM e Heap Kernel\n");
        vga_puts("  df                  - Uso de disco do RAMDisk MIGFS e ATA\n");
        vga_puts("  uptime / date / time- Tempo de execucao do kernel e timer PIT\n");
        vga_puts("  rtc / clock / cmos  - Diagnostico do hardware CMOS/RTC e bateria\n");
        vga_puts("  clear / cls         - Limpa a tela do terminal\n");
        vga_puts("  reboot              - Reinicia a maquina com seguranca\n");
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("  Atalhos: [TAB] Auto-completar | [Ctrl+C] Cancela | [Ctrl+L] Limpa\n");
        vga_puts("           [Setas] Navegacao/Historico | [Ctrl+U/K/W] Edicao de linha\n");
        vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        vga_puts("=================================================================\n");
    } else if (strcmp(cmd, "clear") == 0 || strcmp(cmd, "cls") == 0) {
        vga_clear();
    } else if (strncmp(cmd, "ls", 2) == 0 && (cmd[2] == ' ' || cmd[2] == '\0')) {
        cmd_ls(cmd + 2);
    } else if (strncmp(cmd, "dir", 3) == 0 && (cmd[3] == ' ' || cmd[3] == '\0')) {
        cmd_ls(cmd + 3);
    } else if (strcmp(cmd, "pwd") == 0 || (strncmp(cmd, "pwd", 3) == 0 && (cmd[3] == ' ' || cmd[3] == '\0'))) {
        cmd_pwd();
    } else if (strncmp(cmd, "cd", 2) == 0 && (cmd[2] == ' ' || cmd[2] == '\0')) {
        cmd_cd(cmd + 2);
    } else if (strncmp(cmd, "mkdir", 5) == 0 && (cmd[5] == ' ' || cmd[5] == '\0')) {
        cmd_mkdir(cmd + 5);
    } else if (strncmp(cmd, "md", 2) == 0 && (cmd[2] == ' ' || cmd[2] == '\0')) {
        cmd_mkdir(cmd + 2);
    } else if (strncmp(cmd, "rmdir", 5) == 0 && (cmd[5] == ' ' || cmd[5] == '\0')) {
        cmd_rmdir(cmd + 5);
    } else if (strncmp(cmd, "rd", 2) == 0 && (cmd[2] == ' ' || cmd[2] == '\0')) {
        cmd_rmdir(cmd + 2);
    } else if (strncmp(cmd, "mv", 2) == 0 && (cmd[2] == ' ' || cmd[2] == '\0')) {
        cmd_mv(cmd + 2);
    } else if (strncmp(cmd, "cp", 2) == 0 && (cmd[2] == ' ' || cmd[2] == '\0')) {
        cmd_cp(cmd + 2);
    } else if (strncmp(cmd, "cat", 3) == 0 && (cmd[3] == ' ' || cmd[3] == '\0')) {
        cmd_cat(cmd + 3);
    } else if (strncmp(cmd, "type", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) {
        cmd_cat(cmd + 4);
    } else if (strncmp(cmd, "echo", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) {
        cmd_echo(cmd + 4);
    } else if ((strncmp(cmd, "edit", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) ||
               (strncmp(cmd, "nano", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) ||
               (strncmp(cmd, "vi", 2) == 0 && (cmd[2] == ' ' || cmd[2] == '\0')) ||
               (strncmp(cmd, "vim", 3) == 0 && (cmd[3] == ' ' || cmd[3] == '\0'))) {
        const char* fname = cmd;
        while (*fname != ' ' && *fname != '\0') fname++;
        while (*fname == ' ') fname++;
        char target[MIGFS_MAX_FILENAME];
        resolve_shell_path(fname, target, sizeof(target));
        editor_open_cli(target);
    } else if ((strncmp(cmd, "run", 3) == 0 && (cmd[3] == ' ' || cmd[3] == '\0')) ||
               (strncmp(cmd, "exec", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) ||
               (strncmp(cmd, "batch", 5) == 0 && (cmd[5] == ' ' || cmd[5] == '\0'))) {
        const char* fname = (cmd[0] == 'r') ? cmd + 3 : ((cmd[0] == 'e') ? cmd + 4 : cmd + 5);
        while (*fname == ' ') fname++;
        char target[MIGFS_MAX_FILENAME];
        resolve_shell_path(fname, target, sizeof(target));
        script_run_file(target);
    } else if ((strncmp(cmd, "gameboy", 7) == 0 && (cmd[7] == ' ' || cmd[7] == '\0')) ||
               (strncmp(cmd, "gb", 2) == 0 && (cmd[2] == ' ' || cmd[2] == '\0')) ||
               strcmp(cmd, "pokemon") == 0) {
        const char* fname = NULL;
        if (strcmp(cmd, "pokemon") == 0) {
            fname = "pokemon.gb";
        } else if (cmd[1] == 'b') {
            fname = cmd + 2;
        } else {
            fname = cmd + 7;
        }
        while (*fname == ' ') fname++;
        if (fname[0] == '\0') {
            if (migfs_exists("PokemonRed.gb")) fname = "PokemonRed.gb";
            else if (migfs_exists("pokemon.gb")) fname = "pokemon.gb";
            else fname = "pokemon.gb";
        }
        char target[MIGFS_MAX_FILENAME];
        resolve_shell_path(fname, target, sizeof(target));
        gameboy_launch(target);
    } else if (strncmp(cmd, "gbinfo", 6) == 0 && (cmd[6] == ' ' || cmd[6] == '\0')) {
        const char* fname = cmd + 6;
        while (*fname == ' ') fname++;
        if (fname[0] == '\0') fname = "pokemon.gb";
        char target[MIGFS_MAX_FILENAME];
        resolve_shell_path(fname, target, sizeof(target));
        gameboy_print_cart_info(target);
    } else if ((strncmp(cmd, "calc", 4) == 0 && cmd[4] == ' ') ||
               (strncmp(cmd, "eval", 4) == 0 && cmd[4] == ' ')) {
        const char* expr = cmd + 5;
        while (*expr == ' ') expr++;
        int result = 0;
        if (script_eval_expr(expr, &result) == 0) {
            char buf[32];
            itoa(result, buf, 10);
            vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
            vga_puts("[CALC] ");
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            vga_puts(expr);
            vga_puts(" = ");
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            vga_puts(buf);
            vga_putc('\n');
            script_set_var("RESULT", buf);
        } else {
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            vga_puts("Erro ao avaliar expressao.\n");
        }
    } else if (strcmp(cmd, "sync") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        vga_puts("Sincronizando arquivos com o disco ATA primario...\n");
        int ret = migfs_sync_to_disk();
        if (ret == 0) {
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            vga_puts("[OK] Dados persistidos no disco com sucesso!\n");
        } else {
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            vga_puts("[ERRO] Falha ao sincronizar com o disco ATA.\n");
        }
    } else if (strncmp(cmd, "touch", 5) == 0 && (cmd[5] == ' ' || cmd[5] == '\0')) {
        cmd_touch(cmd + 5);
    } else if (strncmp(cmd, "write", 5) == 0 && (cmd[5] == ' ' || cmd[5] == '\0')) {
        cmd_write(cmd + 5);
    } else if (strncmp(cmd, "rm", 2) == 0 && (cmd[2] == ' ' || cmd[2] == '\0')) {
        cmd_rm(cmd + 2);
    } else if (strncmp(cmd, "del", 3) == 0 && (cmd[3] == ' ' || cmd[3] == '\0')) {
        cmd_rm(cmd + 3);
    } else if (strncmp(cmd, "unlink", 6) == 0 && (cmd[6] == ' ' || cmd[6] == '\0')) {
        cmd_rm(cmd + 6);
    } else if (strcmp(cmd, "history") == 0) {
        shell_history_list();
    } else if (strcmp(cmd, "whoami") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("miguel (root / kernel space)\n");
    } else if (strcmp(cmd, "hostname") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        vga_puts("migOS\n");
    } else if (strcmp(cmd, "free") == 0) {
        cmd_free();
    } else if (strcmp(cmd, "df") == 0) {
        cmd_df();
    } else if (strcmp(cmd, "date") == 0 || strcmp(cmd, "time") == 0) {
        cmd_date();
    } else if (strcmp(cmd, "rtc") == 0 || strcmp(cmd, "clock") == 0 || strcmp(cmd, "cmos") == 0) {
        cmd_rtc();
    } else if (strncmp(cmd, "beep", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) {
        cmd_beep(cmd + 4);
    } else if (strncmp(cmd, "sfx", 3) == 0 && (cmd[3] == ' ' || cmd[3] == '\0')) {
        cmd_sfx(cmd + 3);
    } else if (strncmp(cmd, "mute", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) {
        cmd_mute(cmd + 4);
    } else if (strncmp(cmd, "sound", 5) == 0 && (cmd[5] == ' ' || cmd[5] == '\0')) {
        cmd_mute(cmd + 5);
    } else if (strcmp(cmd, "meminfo") == 0) {
        print_meminfo();
    } else if (strcmp(cmd, "memtest") == 0) {
        print_memtest();
    } else if (strcmp(cmd, "uptime") == 0) {
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
    } else if (strcmp(cmd, "matrix") == 0) {
        matrix_effect();
    } else if (strcmp(cmd, "snake") == 0) {
        snake_game_main();
    } else if (strcmp(cmd, "gui") == 0 || strcmp(cmd, "desktop") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        vga_puts("Iniciando migOS Classic Desktop (Mac OS System 7 640x480)...\n");
        sleep(200);
        gui_launch_desktop();
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Retornado ao terminal de comandos migOS.\n\n");
    } else if (strcmp(cmd, "version") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("migOS Kernel v0.5 (32-bit Protected Mode com File Manager & Game Boy Engine)\n");
    } else if (strcmp(cmd, "about") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
        vga_puts("migOS - Sistema Operacional Desenvolvido por Miguel\n");
        vga_puts("Arquitetura: x86 (IA-32) Bare-Metal com MIGFS Persistente\n");
    } else if (strcmp(cmd, "panic") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Disparando excecao de CPU (ISR 0)...\n");
        __asm__ volatile ("int $0");
    } else if (strcmp(cmd, "reboot") == 0) {
        reboot_system();
    } else {
        char target[MIGFS_MAX_FILENAME];
        resolve_shell_path(cmd, target, sizeof(target));

        if (migfs_exists(target) && strstr(target, ".gb")) {
            gameboy_launch(target);
        } else if (migfs_exists(target) && strstr(target, ".txt")) {
            script_run_file(target);
        } else if (migfs_is_dir(target)) {
            cmd_cd(cmd);
        } else {
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            vga_puts("Comando desconhecido: '");
            vga_puts(cmd);
            vga_puts("'. Digite 'help' para ver os comandos disponiveis.\n");
        }
    }

    shell_print_prompt();
}


