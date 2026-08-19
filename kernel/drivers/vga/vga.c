#include <drivers/vga.h>
#include <drivers/bga.h>
#include <gui/font8x8.h>
#include <arch/i386/io.h>

#define VGA_SCROLLBACK_LINES 300

static int cursor_row = 0;
static int cursor_col = 0;
static unsigned char current_color = 0x07;

static unsigned short live_screen[VGA_HEIGHT][VGA_WIDTH];
static unsigned short scrollback[VGA_SCROLLBACK_LINES][VGA_WIDTH];
static int scrollback_head = 0;
static int scrollback_count = 0;
static int scroll_view_offset = 0;

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
    if (cursor_col < 0 || cursor_col >= VGA_WIDTH || cursor_row < 0 || cursor_row >= VGA_HEIGHT) return;
    uint32_t* fb = bga_get_framebuffer();
    if (!fb) return;

    uint32_t fg = vga_colors_32[current_color & 0x0F];
    int start_x = cursor_col * 8;
    int start_y = cursor_row * 16 + 14;

    for (int y = start_y; y < start_y + 2 && y < SCREEN_HEIGHT; y++) {
        for (int x = start_x; x < start_x + 8 && x < SCREEN_WIDTH; x++) {
            fb[y * SCREEN_WIDTH + x] = fg;
        }
    }
}

static void update_hardware_cursor(void) {
    draw_cursor();
}

static void vga_refresh_display(void) {
    if (scroll_view_offset == 0) {
        for (int y = 0; y < VGA_HEIGHT; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                unsigned short entry = live_screen[y][x];
                draw_text_cell(x, y, (unsigned char)(entry & 0xFF), (unsigned char)(entry >> 8));
                VGA_MEMORY[y * VGA_WIDTH + x] = entry;
            }
        }
        draw_cursor();
    } else {
        // Exibe historico retroativo
        for (int y = 0; y < VGA_HEIGHT; y++) {
            int hist_idx = (scrollback_head - scroll_view_offset + y) % VGA_SCROLLBACK_LINES;
            if (hist_idx < 0) hist_idx += VGA_SCROLLBACK_LINES;

            for (int x = 0; x < VGA_WIDTH; x++) {
                if (y < VGA_HEIGHT - 1) {
                    unsigned short entry = scrollback[hist_idx][x];
                    draw_text_cell(x, y, (unsigned char)(entry & 0xFF), (unsigned char)(entry >> 8));
                    VGA_MEMORY[y * VGA_WIDTH + x] = entry;
                } else {
                    // Barra de status na ultima linha
                    unsigned short bar_char = vga_entry(' ', ((VGA_COLOR_DARK_GREY & 0x0F) << 4) | (VGA_COLOR_WHITE & 0x0F));
                    draw_text_cell(x, y, ' ', (unsigned char)(bar_char >> 8));
                    VGA_MEMORY[y * VGA_WIDTH + x] = bar_char;
                }
            }
        }

        const char* msg = " [ SCROLL HISTORICO - Gire o mouse para baixo para retornar ] ";
        for (int i = 0; msg[i] != '\0' && (5 + i) < VGA_WIDTH; i++) {
            unsigned char attr = ((VGA_COLOR_BLUE & 0x0F) << 4) | (VGA_COLOR_YELLOW & 0x0F);
            draw_text_cell(5 + i, VGA_HEIGHT - 1, msg[i], attr);
            VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + 5 + i] = vga_entry(msg[i], attr);
        }
    }
}

void vga_clear(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            live_screen[y][x] = vga_entry(' ', current_color);
            draw_text_cell(x, y, ' ', current_color);
            VGA_MEMORY[y * VGA_WIDTH + x] = live_screen[y][x];
        }
    }
    cursor_row = 0;
    cursor_col = 0;
    scroll_view_offset = 0;
    draw_cursor();
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
    scrollback_head = 0;
    scrollback_count = 0;
    scroll_view_offset = 0;
    vga_clear();
}

void vga_set_color(unsigned char fg, unsigned char bg) {
    current_color = ((bg & 0x0F) << 4) | (fg & 0x0F);
}

void vga_set_cell(int x, int y, char c, unsigned char fg, unsigned char bg) {
    if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT) return;
    unsigned char color = ((bg & 0x0F) << 4) | (fg & 0x0F);
    live_screen[y][x] = vga_entry((unsigned char)c, color);
    draw_text_cell(x, y, (unsigned char)c, color);
    VGA_MEMORY[y * VGA_WIDTH + x] = live_screen[y][x];
}

void vga_set_cursor(int row, int col) {
    // Redesenha a celula anterior para limpar o cursor visual
    if (cursor_col >= 0 && cursor_col < VGA_WIDTH && cursor_row >= 0 && cursor_row < VGA_HEIGHT) {
        unsigned short prev = live_screen[cursor_row][cursor_col];
        draw_text_cell(cursor_col, cursor_row, (unsigned char)(prev & 0xFF), (unsigned char)(prev >> 8));
    }
    cursor_row = row;
    cursor_col = col;
    if (scroll_view_offset == 0) {
        draw_cursor();
    }
}

void vga_scroll(void) {
    // Salva a linha que esta rolando para fora no scrollback
    for (int x = 0; x < VGA_WIDTH; x++) {
        scrollback[scrollback_head][x] = live_screen[0][x];
    }
    scrollback_head = (scrollback_head + 1) % VGA_SCROLLBACK_LINES;
    if (scrollback_count < VGA_SCROLLBACK_LINES) {
        scrollback_count++;
    }

    // Desloca linhas para cima
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            live_screen[y][x] = live_screen[y + 1][x];
        }
    }

    for (int x = 0; x < VGA_WIDTH; x++) {
        live_screen[VGA_HEIGHT - 1][x] = vga_entry(' ', current_color);
    }

    cursor_row = VGA_HEIGHT - 1;

    if (scroll_view_offset == 0) {
        vga_refresh_display();
    }
}

void vga_scroll_history_up(int lines) {
    if (scrollback_count == 0) return;
    scroll_view_offset += lines;
    if (scroll_view_offset > scrollback_count) {
        scroll_view_offset = scrollback_count;
    }
    vga_refresh_display();
}

void vga_scroll_history_down(int lines) {
    if (scroll_view_offset == 0) return;
    scroll_view_offset -= lines;
    if (scroll_view_offset < 0) {
        scroll_view_offset = 0;
    }
    vga_refresh_display();
}

void vga_scroll_history_reset(void) {
    if (scroll_view_offset != 0) {
        scroll_view_offset = 0;
        vga_refresh_display();
    }
}

int vga_get_scroll_offset(void) {
    return scroll_view_offset;
}

static unsigned char utf8_to_cp437(unsigned char b1, unsigned char b2) {
    if (b1 == 0xC3) {
        switch (b2) {
            case 0xA1: return 0xA0; // á
            case 0xA0: return 0x85; // à
            case 0xA3: return 0xC6; // ã -> 'a' com til / CP437 0xC6 (ou approx)
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

    if (scroll_view_offset != 0) {
        scroll_view_offset = 0;
        vga_refresh_display();
    }

    if (uc == '\n') {
        // Redesenha celula anterior para limpar o cursor antes de mudar de linha
        unsigned short prev = live_screen[cursor_row][cursor_col];
        draw_text_cell(cursor_col, cursor_row, (unsigned char)(prev & 0xFF), (unsigned char)(prev >> 8));
        cursor_col = 0;
        cursor_row++;
        if (cursor_row >= VGA_HEIGHT) {
            vga_scroll();
        }
    } else if (uc == '\r') {
        unsigned short prev = live_screen[cursor_row][cursor_col];
        draw_text_cell(cursor_col, cursor_row, (unsigned char)(prev & 0xFF), (unsigned char)(prev >> 8));
        cursor_col = 0;
    } else if (uc == '\b') {
        if (cursor_col > 0) {
            unsigned short prev = live_screen[cursor_row][cursor_col];
            draw_text_cell(cursor_col, cursor_row, (unsigned char)(prev & 0xFF), (unsigned char)(prev >> 8));
            cursor_col--;
            live_screen[cursor_row][cursor_col] = vga_entry(' ', current_color);
            draw_text_cell(cursor_col, cursor_row, ' ', current_color);
            VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] = live_screen[cursor_row][cursor_col];
        }
    } else {
        live_screen[cursor_row][cursor_col] = vga_entry(uc, current_color);
        draw_text_cell(cursor_col, cursor_row, uc, current_color);
        VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] = live_screen[cursor_row][cursor_col];
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
            if (cursor_row >= VGA_HEIGHT) {
                vga_scroll();
            }
        }
    }

    draw_cursor();
}

void vga_puts(const char* str) {
    if (!str) return;
    for (int i = 0; str[i] != '\0'; i++) {
        vga_putc(str[i]);
    }
}
