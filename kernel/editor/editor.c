#include <editor/editor.h>
#include <fs/migfs.h>
#include <interpreter/txt_interp.h>
#include <drivers/vga.h>
#include <drivers/keyboard.h>
#include <kernel/kheap.h>
#include <arch/i386/timer.h>
#include <libc/stdio.h>
#include <libc/stdlib.h>
#include <libc/string.h>

#define MAX_EDITOR_LINES 1024
#define MAX_LINE_LEN     256
#define VIEW_ROWS        26
#define VIEW_COLS        73
#define LINE_NUM_WIDTH   5

typedef struct {
    char chars[MAX_LINE_LEN];
    int len;
} editor_line_t;

static editor_line_t lines[MAX_EDITOR_LINES];
static int line_count = 1;
static int cur_row = 0;
static int cur_col = 0;
static int scroll_row = 0;
static int scroll_col = 0;
static char current_filename[MIGFS_MAX_FILENAME];
static int is_dirty = 0;
static char status_msg[80];
static uint32_t status_timer = 0;

static void editor_set_status(const char* msg) {
    strncpy(status_msg, msg, sizeof(status_msg) - 1);
    status_msg[sizeof(status_msg) - 1] = '\0';
    status_timer = timer_get_ticks();
}

static void editor_load_file(const char* filename) {
    strncpy(current_filename, filename, MIGFS_MAX_FILENAME - 1);
    current_filename[MIGFS_MAX_FILENAME - 1] = '\0';

    line_count = 0;
    cur_row = 0;
    cur_col = 0;
    scroll_row = 0;
    scroll_col = 0;
    is_dirty = 0;

    migfs_file_t* f = migfs_open(filename);
    if (!f || !f->data || f->size == 0) {
        lines[0].chars[0] = '\0';
        lines[0].len = 0;
        line_count = 1;
        editor_set_status("[Novo arquivo] - Digite seu texto");
        return;
    }

    const char* p = f->data;
    lines[0].len = 0;
    lines[0].chars[0] = '\0';
    line_count = 1;

    while (*p != '\0' && line_count <= MAX_EDITOR_LINES) {
        if (*p == '\r') {
            p++;
            continue;
        }
        if (*p == '\n') {
            if (line_count < MAX_EDITOR_LINES) {
                lines[line_count].chars[0] = '\0';
                lines[line_count].len = 0;
                line_count++;
            }
        } else {
            editor_line_t* cl = &lines[line_count - 1];
            if (cl->len < MAX_LINE_LEN - 1) {
                cl->chars[cl->len++] = *p;
                cl->chars[cl->len] = '\0';
            }
        }
        p++;
    }

    char buf[64];
    sprintf(buf, "[Carregado: %u bytes, %d linhas]", (unsigned int)f->size, line_count);
    editor_set_status(buf);
}

static int editor_save_file(void) {
    size_t total_size = 0;
    for (int i = 0; i < line_count; i++) {
        total_size += lines[i].len + 1; // +1 para '\n'
    }

    char* buffer = (char*)kmalloc(total_size + 1);
    if (!buffer) {
        editor_set_status("[ERRO] Falha de memoria ao salvar!");
        return -1;
    }

    size_t pos = 0;
    for (int i = 0; i < line_count; i++) {
        memcpy(buffer + pos, lines[i].chars, lines[i].len);
        pos += lines[i].len;
        if (i < line_count - 1 || lines[i].len > 0) {
            buffer[pos++] = '\n';
        }
    }
    buffer[pos] = '\0';

    int ret = migfs_write(current_filename, buffer, pos);
    kfree(buffer);

    if (ret == 0) {
        is_dirty = 0;
        char buf[80];
        sprintf(buf, "[OK] Salvo e persistido no disco! (%u bytes)", (unsigned int)pos);
        editor_set_status(buf);
        return 0;
    } else {
        editor_set_status("[ERRO] Falha ao gravar arquivo no disco!");
        return -1;
    }
}

static void editor_render(void) {
    // 1. Linha 0: Barra Superior de Titulo
    vga_set_cursor(0, 0);
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_set_cell(x, 0, ' ', VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    }
    char title_buf[80];
    sprintf(title_buf, " migOS Editor 1.0 | Arquivo: %s%s", current_filename, is_dirty ? " *" : "");
    for (int x = 0; title_buf[x] != '\0' && x < 50; x++) {
        vga_set_cell(x, 0, title_buf[x], VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    }
    const char* help_top = "[F2/^S: Salvar] [ESC/^X: Sair] ";
    int top_help_x = VGA_WIDTH - (int)strlen(help_top);
    for (int x = 0; help_top[x] != '\0'; x++) {
        vga_set_cell(top_help_x + x, 0, help_top[x], VGA_COLOR_YELLOW, VGA_COLOR_BLUE);
    }

    // 2. Linhas 1 a 26: Area de Edicao de Texto com Numero de Linha
    for (int r = 0; r < VIEW_ROWS; r++) {
        int v_row = 1 + r;
        int f_line = scroll_row + r;

        if (f_line < line_count) {
            // Numero da linha
            char num_str[16];
            sprintf(num_str, "%4d|", f_line + 1);
            for (int i = 0; i < LINE_NUM_WIDTH; i++) {
                vga_set_cell(i, v_row, num_str[i], VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
            }

            // Caracteres do texto da linha
            editor_line_t* cl = &lines[f_line];
            for (int c = 0; c < VIEW_COLS; c++) {
                int ch_idx = scroll_col + c;
                char ch = (ch_idx < cl->len) ? cl->chars[ch_idx] : ' ';
                vga_set_cell(LINE_NUM_WIDTH + c, v_row, ch, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            }
        } else {
            // Linhas vazias alem do fim do arquivo
            vga_set_cell(0, v_row, '~', VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
            for (int c = 1; c < VGA_WIDTH; c++) {
                vga_set_cell(c, v_row, ' ', VGA_COLOR_BLACK, VGA_COLOR_BLACK);
            }
        }
    }

    // 3. Linha 27: Barra de Status
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_set_cell(x, 27, ' ', VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    }
    char stat[80];
    sprintf(stat, " Lin %d/%d, Col %d | %s", cur_row + 1, line_count, cur_col + 1, status_msg);
    for (int x = 0; stat[x] != '\0' && x < VGA_WIDTH; x++) {
        vga_set_cell(x, 27, stat[x], VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    }

    // 4. Linha 28-29: Atalhos
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_set_cell(x, 28, ' ', VGA_COLOR_WHITE, VGA_COLOR_BLUE);
        vga_set_cell(x, 29, ' ', VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    }
    const char* bar1 = " ^S/F2: Salvar Disco   ^X/ESC: Sair   ^R: Executar Script (.txt)";
    const char* bar2 = " ^K: Limpar Tudo       Tab: 4 Espacos  Setas/Home/End/PgUp/PgDn: Navegar";
    for (int x = 0; bar1[x] != '\0' && x < VGA_WIDTH; x++) {
        vga_set_cell(x, 28, bar1[x], VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    }
    for (int x = 0; bar2[x] != '\0' && x < VGA_WIDTH; x++) {
        vga_set_cell(x, 29, bar2[x], VGA_COLOR_YELLOW, VGA_COLOR_BLUE);
    }

    // Posiciona o cursor de video VGA na tela
    int scr_cx = LINE_NUM_WIDTH + (cur_col - scroll_col);
    int scr_cy = 1 + (cur_row - scroll_row);
    if (scr_cx >= LINE_NUM_WIDTH && scr_cx < VGA_WIDTH && scr_cy >= 1 && scr_cy <= VIEW_ROWS) {
        vga_set_cursor(scr_cy, scr_cx);
    }
}

static void editor_adjust_scroll(void) {
    if (cur_row < scroll_row) {
        scroll_row = cur_row;
    }
    if (cur_row >= scroll_row + VIEW_ROWS) {
        scroll_row = cur_row - VIEW_ROWS + 1;
    }
    if (cur_col < scroll_col) {
        scroll_col = cur_col;
    }
    if (cur_col >= scroll_col + VIEW_COLS) {
        scroll_col = cur_col - VIEW_COLS + 1;
    }
}

static void editor_insert_char(char c) {
    editor_line_t* cl = &lines[cur_row];
    if (cl->len < MAX_LINE_LEN - 1) {
        for (int i = cl->len; i > cur_col; i--) {
            cl->chars[i] = cl->chars[i - 1];
        }
        cl->chars[cur_col] = c;
        cl->len++;
        cl->chars[cl->len] = '\0';
        cur_col++;
        is_dirty = 1;
    }
}

static void editor_insert_newline(void) {
    if (line_count >= MAX_EDITOR_LINES) return;

    // Desloca linhas para baixo
    for (int i = line_count; i > cur_row + 1; i--) {
        lines[i] = lines[i - 1];
    }

    editor_line_t* cur = &lines[cur_row];
    editor_line_t* next = &lines[cur_row + 1];

    // Transfere restante do texto para a nova linha
    int rem_len = cur->len - cur_col;
    memcpy(next->chars, cur->chars + cur_col, rem_len);
    next->len = rem_len;
    next->chars[rem_len] = '\0';

    cur->len = cur_col;
    cur->chars[cur_col] = '\0';

    line_count++;
    cur_row++;
    cur_col = 0;
    is_dirty = 1;
}

static void editor_delete_backspace(void) {
    if (cur_col > 0) {
        editor_line_t* cl = &lines[cur_row];
        for (int i = cur_col - 1; i < cl->len; i++) {
            cl->chars[i] = cl->chars[i + 1];
        }
        cl->len--;
        cur_col--;
        is_dirty = 1;
    } else if (cur_row > 0) {
        // Junta linha atual com a anterior
        editor_line_t* prev = &lines[cur_row - 1];
        editor_line_t* cur = &lines[cur_row];

        if (prev->len + cur->len < MAX_LINE_LEN - 1) {
            int old_prev_len = prev->len;
            memcpy(prev->chars + prev->len, cur->chars, cur->len);
            prev->len += cur->len;
            prev->chars[prev->len] = '\0';

            // Remove linha atual
            for (int i = cur_row; i < line_count - 1; i++) {
                lines[i] = lines[i + 1];
            }
            line_count--;
            cur_row--;
            cur_col = old_prev_len;
            is_dirty = 1;
        }
    }
}

static void editor_delete_char(void) {
    editor_line_t* cl = &lines[cur_row];
    if (cur_col < cl->len) {
        for (int i = cur_col; i < cl->len; i++) {
            cl->chars[i] = cl->chars[i + 1];
        }
        cl->len--;
        is_dirty = 1;
    } else if (cur_row < line_count - 1) {
        // Junta proxima linha com a atual
        editor_line_t* next = &lines[cur_row + 1];
        if (cl->len + next->len < MAX_LINE_LEN - 1) {
            memcpy(cl->chars + cl->len, next->chars, next->len);
            cl->len += next->len;
            cl->chars[cl->len] = '\0';

            for (int i = cur_row + 1; i < line_count - 1; i++) {
                lines[i] = lines[i + 1];
            }
            line_count--;
            is_dirty = 1;
        }
    }
}

void editor_open_cli(const char* filename) {
    if (!filename || filename[0] == '\0') {
        filename = "sem_titulo.txt";
    }

    vga_clear();
    keyboard_set_doom_mode(1);
    editor_load_file(filename);

    int running = 1;

    while (running) {
        editor_adjust_scroll();
        editor_render();

        key_event_t ev;
        while (!keyboard_get_event(&ev)) {
            __asm__ volatile ("hlt");
        }

        if (!ev.pressed) continue;

        // Comandos de Controle (Ctrl+Key)
        if (ev.ctrl) {
            if (ev.key == 's' || ev.key == 'S') {
                editor_save_file();
                continue;
            } else if (ev.key == 'x' || ev.key == 'X' || ev.key == 'q' || ev.key == 'Q') {
                running = 0;
                break;
            } else if (ev.key == 'r' || ev.key == 'R') {
                editor_save_file();
                vga_clear();
                script_run_file(current_filename);
                vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
                vga_puts("\nPressione qualquer tecla para retornar ao editor...");
                keyboard_clear_key();
                while (!keyboard_has_key()) {
                    __asm__ volatile ("hlt");
                }
                keyboard_clear_key();
                vga_clear();
                continue;
            } else if (ev.key == 'k' || ev.key == 'K') {
                lines[0].chars[0] = '\0';
                lines[0].len = 0;
                line_count = 1;
                cur_row = 0;
                cur_col = 0;
                is_dirty = 1;
                editor_set_status("[Texto limpo]");
                continue;
            }
        }

        // Teclas especiais
        if (ev.key == KEY_F2) {
            editor_save_file();
        } else if (ev.key == KEY_ESCAPE) {
            running = 0;
        } else if (ev.key == KEY_UP_ARROW) {
            if (cur_row > 0) {
                cur_row--;
                if (cur_col > lines[cur_row].len) cur_col = lines[cur_row].len;
            }
        } else if (ev.key == KEY_DOWN_ARROW) {
            if (cur_row < line_count - 1) {
                cur_row++;
                if (cur_col > lines[cur_row].len) cur_col = lines[cur_row].len;
            }
        } else if (ev.key == KEY_LEFT_ARROW) {
            if (cur_col > 0) {
                cur_col--;
            } else if (cur_row > 0) {
                cur_row--;
                cur_col = lines[cur_row].len;
            }
        } else if (ev.key == KEY_RIGHT_ARROW) {
            if (cur_col < lines[cur_row].len) {
                cur_col++;
            } else if (cur_row < line_count - 1) {
                cur_row++;
                cur_col = 0;
            }
        } else if (ev.key == KEY_HOME) {
            cur_col = 0;
        } else if (ev.key == KEY_END) {
            cur_col = lines[cur_row].len;
        } else if (ev.key == KEY_PAGEUP) {
            cur_row -= 20;
            if (cur_row < 0) cur_row = 0;
            if (cur_col > lines[cur_row].len) cur_col = lines[cur_row].len;
        } else if (ev.key == KEY_PAGEDOWN) {
            cur_row += 20;
            if (cur_row >= line_count) cur_row = line_count - 1;
            if (cur_col > lines[cur_row].len) cur_col = lines[cur_row].len;
        } else if (ev.key == KEY_BACKSPACE) {
            editor_delete_backspace();
        } else if (ev.key == KEY_DELETE) {
            editor_delete_char();
        } else if (ev.key == KEY_ENTER) {
            editor_insert_newline();
        } else if (ev.key == KEY_TAB) {
            for (int t = 0; t < 4; t++) editor_insert_char(' ');
        } else if (ev.key >= 32 && ev.key <= 126) {
            editor_insert_char((char)ev.key);
        }
    }

    keyboard_set_doom_mode(0);
    vga_clear();
}
