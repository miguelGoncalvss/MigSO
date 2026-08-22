#include <drivers/vga.h>
#include <drivers/bga.h>
#include <gui/font8x8.h>
#include <arch/i386/io.h>

#define TERM_BUFFER_LINES 500

static int term_cursor_row = 0; // Linha atual de escrita no buffer (0 .. TERM_BUFFER_LINES - 1)
static int term_cursor_col = 0; // Coluna atual de escrita no buffer (0 .. VGA_WIDTH - 2)
static int term_view_row = 0;   // Primeira linha do buffer visivel na tela (0 .. term_cursor_row)
static unsigned char current_color = 0x07;

static unsigned short term_buffer[TERM_BUFFER_LINES][VGA_WIDTH];

static const uint32_t vga_colors_32[16] = {
    0x00000000, // 0: Black
    0x000000AA, // 1: Blue
    0x0000AA00, // 2: Green
    0x0000AAAA, // 3: Cyan
    0x00AA0000, // 4: Red
    0x00AA00AA, // 5: Magenta
    0x00AA5500, // 6: Brown
    0x00AAAAAA, // 7: Light Gray
    0x00555555, // 8: Dark Gray
    0x005555FF, // 9: Light Blue
    0x0055FF55, // 10: Light Green
    0x0055FFFF, // 11: Light Cyan
    0x00FF5555, // 12: Light Red
    0x00FF55FF, // 13: Light Magenta
    0x00FFFF55, // 14: Yellow
    0x00FFFFFF  // 15: White
};

static inline unsigned short vga_entry(unsigned char ch, unsigned char color) {
    return (unsigned short)ch | ((unsigned short)color << 8);
}

static void draw_text_cell(int col, int row, unsigned char ch, unsigned char color_attr) {
    if (col < 0 || col >= VGA_WIDTH || row < 0 || row >= VGA_HEIGHT) return;
    uint32_t* fb = bga_get_framebuffer();
    if (!fb) return;

    uint32_t fg = vga_colors_32[color_attr & 0x0F];
    uint32_t bg = vga_colors_32[(color_attr >> 4) & 0x0F];

    unsigned char uc = (unsigned char)ch;
    if (uc > 127) uc = ' ';
    const uint8_t* glyph = font8x8_basic[uc];
    int start_x = col * 8;
    int start_y = row * 16;

    for (int r = 0; r < 8; r++) {
        uint8_t bits = glyph[r];
        int py1 = start_y + (r * 2);
        int py2 = py1 + 1;
        for (int c = 0; c < 8; c++) {
            uint32_t color = (bits & (1 << (7 - c))) ? fg : bg;
            int px = start_x + c;
            fb[py1 * SCREEN_WIDTH + px] = color;
            fb[py2 * SCREEN_WIDTH + px] = color;
        }
    }
}

static void draw_cursor(void) {
    int screen_r = term_cursor_row - term_view_row;
    if (screen_r < 0 || screen_r >= VGA_HEIGHT) return;
    if (term_cursor_col < 0 || term_cursor_col >= VGA_WIDTH - 1) return;

    uint32_t* fb = bga_get_framebuffer();
    if (!fb) return;

    uint32_t fg = vga_colors_32[current_color & 0x0F];
    int start_x = term_cursor_col * 8;
    int start_y = screen_r * 16 + 14;

    for (int y = start_y; y < start_y + 2 && y < SCREEN_HEIGHT; y++) {
        for (int x = start_x; x < start_x + 8 && x < SCREEN_WIDTH; x++) {
            fb[y * SCREEN_WIDTH + x] = fg;
        }
    }
}

static void vga_draw_scrollbar(int max_view) {
    int col = VGA_WIDTH - 1; // Coluna 79 (Borda lateral direita)

    // Botao Topo (▲ = char 30)
    unsigned char arrow_attr = ((VGA_COLOR_DARK_GREY & 0x0F) << 4) | (VGA_COLOR_LIGHT_CYAN & 0x0F);
    draw_text_cell(col, 0, 30, arrow_attr);
    VGA_MEMORY[0 * VGA_WIDTH + col] = vga_entry(30, arrow_attr);

    // Botao Fundo (▼ = char 31)
    draw_text_cell(col, VGA_HEIGHT - 1, 31, arrow_attr);
    VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = vga_entry(31, arrow_attr);

    int track_len = VGA_HEIGHT - 2; // 28 linhas (linhas 1 a 28)
    unsigned char track_attr = ((VGA_COLOR_BLACK & 0x0F) << 4) | (VGA_COLOR_DARK_GREY & 0x0F);
    unsigned char thumb_attr = ((VGA_COLOR_DARK_GREY & 0x0F) << 4) | (VGA_COLOR_LIGHT_CYAN & 0x0F);

    if (max_view == 0) {
        // Tudo cabe na tela visivel: slider preenche a trilha
        for (int y = 1; y <= track_len; y++) {
            draw_text_cell(col, y, 127, thumb_attr);
            VGA_MEMORY[y * VGA_WIDTH + col] = vga_entry(127, thumb_attr);
        }
    } else {
        int total_lines = term_cursor_row + 1;
        int thumb_size = (track_len * VGA_HEIGHT) / total_lines;
        if (thumb_size < 2) thumb_size = 2;
        if (thumb_size > track_len - 2) thumb_size = track_len - 2;

        int max_travel = track_len - thumb_size;
        int thumb_top = 1 + (term_view_row * max_travel) / max_view;
        if (thumb_top < 1) thumb_top = 1;
        if (thumb_top + thumb_size > 1 + track_len) thumb_top = 1 + track_len - thumb_size;

        for (int y = 1; y <= track_len; y++) {
            if (y >= thumb_top && y < thumb_top + thumb_size) {
                draw_text_cell(col, y, 127, thumb_attr);
                VGA_MEMORY[y * VGA_WIDTH + col] = vga_entry(127, thumb_attr);
            } else {
                draw_text_cell(col, y, 124, track_attr);
                VGA_MEMORY[y * VGA_WIDTH + col] = vga_entry(124, track_attr);
            }
        }
    }
}

static void vga_refresh_display(void) {
    int max_view = term_cursor_row - (VGA_HEIGHT - 1);
    if (max_view < 0) max_view = 0;

    // 1. Redesenha celulas visiveis do buffer
    for (int y = 0; y < VGA_HEIGHT; y++) {
        int buffer_y = term_view_row + y;
        for (int x = 0; x < VGA_WIDTH - 1; x++) {
            unsigned short entry;
            if (buffer_y < TERM_BUFFER_LINES) {
                entry = term_buffer[buffer_y][x];
            } else {
                entry = vga_entry(' ', current_color);
            }
            draw_text_cell(x, y, (unsigned char)(entry & 0xFF), (unsigned char)(entry >> 8));
            VGA_MEMORY[y * VGA_WIDTH + x] = entry;
        }
    }

    // 2. Desenha a barra lateral
    vga_draw_scrollbar(max_view);

    // 3. Se estiver visualizando linhas passadas, exibe o indicador [^ +N lin]
    if (term_view_row < max_view) {
        int diff = max_view - term_view_row;
        char tag[24];
        int pos = 0;
        tag[pos++] = ' ';
        tag[pos++] = '[';
        tag[pos++] = '^';
        tag[pos++] = ' ';
        tag[pos++] = '+';
        char num_buf[10];
        int n_idx = 0;
        int temp = diff;
        while (temp > 0 && n_idx < 9) {
            num_buf[n_idx++] = '0' + (temp % 10);
            temp /= 10;
        }
        for (int k = n_idx - 1; k >= 0; k--) {
            tag[pos++] = num_buf[k];
        }
        tag[pos++] = ' ';
        tag[pos++] = 'l';
        tag[pos++] = 'i';
        tag[pos++] = 'n';
        tag[pos++] = ']';
        tag[pos++] = ' ';
        tag[pos] = '\0';

        int tag_start = VGA_WIDTH - 1 - pos;
        unsigned char tag_attr = ((VGA_COLOR_BLUE & 0x0F) << 4) | (VGA_COLOR_YELLOW & 0x0F);
        for (int i = 0; i < pos; i++) {
            draw_text_cell(tag_start + i, 0, tag[i], tag_attr);
            VGA_MEMORY[0 * VGA_WIDTH + tag_start + i] = vga_entry(tag[i], tag_attr);
        }
    } else {
        draw_cursor();
    }
}

void vga_clear(void) {
    for (int y = 0; y < TERM_BUFFER_LINES; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            term_buffer[y][x] = vga_entry(' ', current_color);
        }
    }
    term_cursor_row = 0;
    term_cursor_col = 0;
    term_view_row = 0;
    vga_refresh_display();
}

static inline void serial_init(void) {
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x80);
    outb(0x3F8, 0x03);
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x03);
    outb(0x3FA, 0xC7);
    outb(0x3FC, 0x0B);
}

static inline void serial_putc(char c) {
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, (unsigned char)c);
}

void vga_init(void) {
    serial_init();
    bga_init();
    current_color = ((VGA_COLOR_BLACK & 0x0F) << 4) | (VGA_COLOR_LIGHT_GREEN & 0x0F);
    vga_clear();
}

void vga_set_color(unsigned char fg, unsigned char bg) {
    current_color = ((bg & 0x0F) << 4) | (fg & 0x0F);
}

void vga_set_cell(int x, int y, char c, unsigned char fg, unsigned char bg) {
    if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT) return;
    int buffer_y = term_view_row + y;
    if (buffer_y < 0 || buffer_y >= TERM_BUFFER_LINES) return;
    unsigned char color = ((bg & 0x0F) << 4) | (fg & 0x0F);
    term_buffer[buffer_y][x] = vga_entry((unsigned char)c, color);
    draw_text_cell(x, y, (unsigned char)c, color);
    VGA_MEMORY[y * VGA_WIDTH + x] = term_buffer[buffer_y][x];
}

void vga_set_cursor(int row, int col) {
    if (col >= VGA_WIDTH - 1) col = VGA_WIDTH - 2;
    if (col < 0) col = 0;
    term_cursor_col = col;

    if (row >= VGA_HEIGHT) row = VGA_HEIGHT - 1;
    if (row < 0) row = 0;
    term_cursor_row = term_view_row + row;
    if (term_cursor_row >= TERM_BUFFER_LINES) {
        term_cursor_row = TERM_BUFFER_LINES - 1;
    }

    vga_refresh_display();
}

void vga_get_cursor(int* row, int* col) {
    if (row) {
        int screen_r = term_cursor_row - term_view_row;
        if (screen_r < 0) screen_r = 0;
        if (screen_r >= VGA_HEIGHT) screen_r = VGA_HEIGHT - 1;
        *row = screen_r;
    }
    if (col) {
        *col = term_cursor_col;
    }
}

void vga_scroll(void) {
    if (term_cursor_row < TERM_BUFFER_LINES - 1) {
        term_cursor_row++;
    } else {
        for (int y = 0; y < TERM_BUFFER_LINES - 1; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                term_buffer[y][x] = term_buffer[y + 1][x];
            }
        }
        for (int x = 0; x < VGA_WIDTH; x++) {
            term_buffer[TERM_BUFFER_LINES - 1][x] = vga_entry(' ', current_color);
        }
    }

    for (int x = 0; x < VGA_WIDTH; x++) {
        term_buffer[term_cursor_row][x] = vga_entry(' ', current_color);
    }

    int max_view = term_cursor_row - (VGA_HEIGHT - 1);
    if (max_view < 0) max_view = 0;
    term_view_row = max_view;

    vga_refresh_display();
}

void vga_scroll_history_up(int lines) {
    if (lines <= 0) return;
    term_view_row -= lines;
    if (term_view_row < 0) {
        term_view_row = 0;
    }
    vga_refresh_display();
}

void vga_scroll_history_down(int lines) {
    if (lines <= 0) return;
    int max_view = term_cursor_row - (VGA_HEIGHT - 1);
    if (max_view < 0) max_view = 0;
    term_view_row += lines;
    if (term_view_row > max_view) {
        term_view_row = max_view;
    }
    vga_refresh_display();
}

void vga_scroll_history_reset(void) {
    int max_view = term_cursor_row - (VGA_HEIGHT - 1);
    if (max_view < 0) max_view = 0;
    if (term_view_row != max_view) {
        term_view_row = max_view;
        vga_refresh_display();
    }
}

int vga_get_scroll_offset(void) {
    int max_view = term_cursor_row - (VGA_HEIGHT - 1);
    if (max_view < 0) max_view = 0;
    return max_view - term_view_row;
}

static unsigned char utf8_to_cp437(unsigned char b1, unsigned char b2) {
    if (b1 == 0xC3) {
        switch (b2) {
            case 0xA1: return 0xA0; // á
            case 0xA0: return 0x85; // à
            case 0xA3: return 0xC6; // ã
            case 0xA2: return 0x83; // â
            case 0xA9: return 0x82; // é
            case 0xA8: return 0x8A; // è
            case 0xAA: return 0x88; // ê
            case 0xAD: return 0xA1; // í
            case 0xAC: return 0x8D; // ì
            case 0xB3: return 0xA2; // ó
            case 0xB2: return 0x95; // ò
            case 0xB5: return 0xE4; // õ
            case 0xB4: return 0x93; // ô
            case 0xBA: return 0xA3; // ú
            case 0xB9: return 0x97; // ù
            case 0xBC: return 0x81; // ü
            case 0xA7: return 0x87; // ç
            case 0x81: return 0x41; // Á
            case 0x89: return 0x90; // É
            case 0x8D: return 0x49; // Í
            case 0x93: return 0x4F; // Ó
            case 0x9A: return 0x55; // Ú
            case 0x87: return 0x80; // Ç
            default:   return b2;
        }
    }
    return b2;
}

void vga_putc(char c) {
    static unsigned char utf8_lead = 0;
    unsigned char uc = (unsigned char)c;

    if (utf8_lead != 0) {
        uc = utf8_to_cp437(utf8_lead, uc);
        utf8_lead = 0;
    } else if (uc >= 0xC0 && uc <= 0xDF) {
        utf8_lead = uc;
        return;
    }

    serial_putc((char)uc);

    // Garante que o scroll volte ao vivo ao imprimir
    int max_view = term_cursor_row - (VGA_HEIGHT - 1);
    if (max_view < 0) max_view = 0;
    if (term_view_row != max_view) {
        term_view_row = max_view;
    }

    if (uc == '\n') {
        term_cursor_col = 0;
        vga_scroll();
        return;
    } else if (uc == '\r') {
        term_cursor_col = 0;
        vga_refresh_display();
        return;
    } else if (uc == '\b') {
        if (term_cursor_col > 0) {
            term_cursor_col--;
            term_buffer[term_cursor_row][term_cursor_col] = vga_entry(' ', current_color);
        }
    } else {
        term_buffer[term_cursor_row][term_cursor_col] = vga_entry(uc, current_color);
        term_cursor_col++;
        if (term_cursor_col >= VGA_WIDTH - 1) {
            term_cursor_col = 0;
            vga_scroll();
            return;
        }
    }

    vga_refresh_display();
}

void vga_puts(const char* str) {
    if (!str) return;
    for (int i = 0; str[i] != '\0'; i++) {
        vga_putc(str[i]);
    }
}
