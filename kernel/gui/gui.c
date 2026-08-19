#include <gui/gui.h>
#include <drivers/bga.h>
#include <drivers/vga.h>
#include <drivers/mouse.h>
#include <drivers/keyboard.h>
#include <arch/i386/timer.h>
#include <arch/i386/reboot.h>
#include <kernel/kheap.h>
#include <kernel/pmm.h>
#include <fs/migfs.h>
#include <games/snake.h>
#include <libc/stdio.h>
#include <libc/stdlib.h>
#include "font8x8.h"

static uint32_t* backbuffer = NULL;

// Sprite 12x18 do cursor classico System 7
// 0 = Transparente, 1 = Borda Preta, 2 = Preenchimento Branco
static const uint8_t cursor_sprite[18][12] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0,0},
    {1,2,2,2,2,2,2,2,2,1,0,0},
    {1,2,2,2,2,2,1,1,1,1,1,0},
    {1,2,2,1,2,2,1,0,0,0,0,0},
    {1,2,1,0,1,2,2,1,0,0,0,0},
    {1,1,0,0,1,2,2,1,0,0,0,0},
    {1,0,0,0,0,1,2,2,1,0,0,0},
    {0,0,0,0,0,1,2,2,1,0,0,0},
    {0,0,0,0,0,0,1,1,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0}
};

// Icone Hard Drive 24x18
// 0 = Transp, 1 = Preto, 2 = Branco, 3 = Cinza Medio, 4 = Cinza Escuro
static const uint8_t icon_hd_24x18[18][24] = {
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,1,1,2,1},
    {1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,1,1,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {0,1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,0},
    {0,1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,0},
    {0,0,1,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

// Icone Snake 24x18
static const uint8_t icon_snake_24x18[18][24] = {
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,1,1,1,1,2,2,2,2,2,2,1,1,1,1,2,2,2,2,2,2,1},
    {1,2,1,2,2,2,2,1,2,2,2,2,1,2,2,2,2,1,2,2,2,2,2,1},
    {1,2,1,2,2,2,2,1,2,2,2,2,1,2,2,2,2,1,2,2,2,2,2,1},
    {1,2,2,1,1,2,2,2,1,1,1,1,2,2,2,2,2,1,2,2,2,2,2,1},
    {1,2,2,2,2,1,1,2,2,2,2,2,2,2,2,2,2,1,2,2,2,2,2,1},
    {1,2,1,1,1,2,2,1,1,1,1,1,1,2,2,2,2,1,2,2,2,2,2,1},
    {1,2,1,2,2,1,2,2,2,2,2,2,2,1,2,2,2,1,2,2,2,2,2,1},
    {1,2,1,2,2,2,1,1,1,1,1,1,2,1,2,2,2,1,2,2,2,2,2,1},
    {1,2,1,1,1,1,2,2,2,2,2,2,1,1,2,2,2,1,2,2,2,2,2,1},
    {1,2,2,2,2,2,1,1,1,1,1,1,2,2,2,2,1,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {0,1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,0},
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

// Icone Terminal / CLI 24x18
static const uint8_t icon_term_24x18[18][24] = {
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1},
    {1,2,1,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,1,2,1},
    {1,2,1,4,2,2,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,1,2,1},
    {1,2,1,4,4,2,2,4,4,4,4,4,4,4,4,4,4,4,4,4,4,1,2,1},
    {1,2,1,4,4,4,2,2,4,4,4,4,4,4,4,4,4,4,4,4,4,1,2,1},
    {1,2,1,4,4,2,2,4,4,4,4,4,4,4,4,4,4,4,4,4,4,1,2,1},
    {1,2,1,4,2,2,4,4,4,4,2,2,2,2,2,4,4,4,4,4,4,1,2,1},
    {1,2,1,4,4,4,4,4,4,4,2,2,2,2,2,4,4,4,4,4,4,1,2,1},
    {1,2,1,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,1,2,1},
    {1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {0,1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,0},
    {0,0,1,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

// Icone Lixeira 20x22
static const uint8_t icon_trash_20x22[22][20] = {
    {0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0},
    {0,0,0,1,1,1,2,2,2,2,2,2,2,2,1,1,1,0,0,0},
    {0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
    {0,0,0,1,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0},
    {0,0,0,1,2,1,2,1,2,1,2,1,2,1,2,2,1,0,0,0},
    {0,0,0,1,2,1,2,1,2,1,2,1,2,1,2,2,1,0,0,0},
    {0,0,0,1,2,1,2,1,2,1,2,1,2,1,2,2,1,0,0,0},
    {0,0,0,1,2,1,2,1,2,1,2,1,2,1,2,2,1,0,0,0},
    {0,0,0,1,2,1,2,1,2,1,2,1,2,1,2,2,1,0,0,0},
    {0,0,0,1,2,1,2,1,2,1,2,1,2,1,2,2,1,0,0,0},
    {0,0,0,1,2,1,2,1,2,1,2,1,2,1,2,2,1,0,0,0},
    {0,0,0,1,2,1,2,1,2,1,2,1,2,1,2,2,1,0,0,0},
    {0,0,0,1,2,1,2,1,2,1,2,1,2,1,2,2,1,0,0,0},
    {0,0,0,1,2,1,2,1,2,1,2,1,2,1,2,2,1,0,0,0},
    {0,0,0,1,2,1,2,1,2,1,2,1,2,1,2,2,1,0,0,0},
    {0,0,0,1,2,1,2,1,2,1,2,1,2,1,2,2,1,0,0,0},
    {0,0,0,1,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0},
    {0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0},
    {0,0,0,0,0,1,4,4,4,4,4,4,4,4,1,0,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

void gui_draw_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < GUI_WIDTH && y >= 0 && y < GUI_HEIGHT) {
        backbuffer[y * GUI_WIDTH + x] = color;
    }
}

void gui_draw_line_h(int x, int y, int w, uint32_t color) {
    if (y < 0 || y >= GUI_HEIGHT) return;
    for (int i = 0; i < w; i++) {
        int px = x + i;
        if (px >= 0 && px < GUI_WIDTH) {
            backbuffer[y * GUI_WIDTH + px] = color;
        }
    }
}

void gui_draw_line_v(int x, int y, int h, uint32_t color) {
    if (x < 0 || x >= GUI_WIDTH) return;
    for (int i = 0; i < h; i++) {
        int py = y + i;
        if (py >= 0 && py < GUI_HEIGHT) {
            backbuffer[py * GUI_WIDTH + x] = color;
        }
    }
}

void gui_draw_rect_fill(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    int x1 = x < 0 ? 0 : x;
    int y1 = y < 0 ? 0 : y;
    int x2 = x + w;
    int y2 = y + h;
    if (x2 > GUI_WIDTH)  x2 = GUI_WIDTH;
    if (y2 > GUI_HEIGHT) y2 = GUI_HEIGHT;

    for (int j = y1; j < y2; j++) {
        for (int i = x1; i < x2; i++) {
            backbuffer[j * GUI_WIDTH + i] = color;
        }
    }
}

void gui_draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    gui_draw_line_h(x, y, w, color);
    gui_draw_line_h(x, y + h - 1, w, color);
    gui_draw_line_v(x, y, h, color);
    gui_draw_line_v(x + w - 1, y, h, color);
}

void gui_draw_char(int x, int y, char c, uint32_t fg) {
    if ((unsigned char)c > 127) return;
    const uint8_t* glyph = font8x8_basic[(unsigned char)c];
    for (int cy = 0; cy < 8; cy++) {
        int py = y + (cy * 2);
        uint8_t row = glyph[cy];
        for (int cx = 0; cx < 8; cx++) {
            int px = x + cx;
            if (row & (1 << (7 - cx))) {
                gui_draw_pixel(px, py, fg);
                gui_draw_pixel(px, py + 1, fg);
            }
        }
    }
}

void gui_draw_char_clipped(int x, int y, char c, uint32_t fg, int min_x, int max_x, int min_y, int max_y) {
    if ((unsigned char)c > 127) return;
    const uint8_t* glyph = font8x8_basic[(unsigned char)c];
    for (int cy = 0; cy < 8; cy++) {
        int py1 = y + (cy * 2);
        int py2 = py1 + 1;
        uint8_t row = glyph[cy];
        for (int cx = 0; cx < 8; cx++) {
            int px = x + cx;
            if (px < min_x || px > max_x) continue;
            if (row & (1 << (7 - cx))) {
                if (py1 >= min_y && py1 <= max_y) gui_draw_pixel(px, py1, fg);
                if (py2 >= min_y && py2 <= max_y) gui_draw_pixel(px, py2, fg);
            }
        }
    }
}

void gui_draw_string(int x, int y, const char* str, uint32_t fg) {
    if (!str) return;
    for (int i = 0; str[i] != '\0'; i++) {
        gui_draw_char(x + (i * 8), y, str[i], fg);
    }
}

void gui_draw_string_clipped(int x, int y, const char* str, uint32_t fg, int max_x) {
    if (!str) return;
    for (int i = 0; str[i] != '\0'; i++) {
        int px = x + (i * 8);
        if (px > max_x) break;
        if (px + 7 > max_x) {
            gui_draw_char_clipped(px, y, str[i], fg, 0, max_x, 0, GUI_HEIGHT - 1);
        } else {
            gui_draw_char(px, y, str[i], fg);
        }
    }
}

void gui_draw_string_win(gui_window_t* win, int rel_x, int rel_y, const char* str, uint32_t fg) {
    if (!win || !win->is_open || !str) return;
    int abs_x = win->x + rel_x;
    int abs_y = win->y + rel_y;
    int max_x = win->x + win->w - 6;
    gui_draw_string_clipped(abs_x, abs_y, str, fg, max_x);
}

static void draw_desktop_background(void) {
    // Padrao 50% Gray Stipple classico do Mac OS System 7
    for (int y = 0; y < GUI_HEIGHT; y++) {
        for (int x = 0; x < GUI_WIDTH; x++) {
            backbuffer[y * GUI_WIDTH + x] = ((x + y) % 2 == 0) ? GUI_COLOR_LIGHT_GRAY : GUI_COLOR_WHITE;
        }
    }
}

static void draw_icon_24x18(int x, int y, const uint8_t icon[18][24]) {
    uint32_t pal[5] = {0, GUI_COLOR_BLACK, GUI_COLOR_WHITE, GUI_COLOR_MID_GRAY, GUI_COLOR_DARK_GRAY};
    for (int j = 0; j < 18; j++) {
        for (int i = 0; i < 24; i++) {
            uint8_t px = icon[j][i];
            if (px > 0 && px < 5) {
                gui_draw_pixel(x + i, y + j, pal[px]);
            }
        }
    }
}

static void draw_icon_trash_20x22(int x, int y, const uint8_t icon[22][20]) {
    uint32_t pal[5] = {0, GUI_COLOR_BLACK, GUI_COLOR_WHITE, GUI_COLOR_MID_GRAY, GUI_COLOR_DARK_GRAY};
    for (int j = 0; j < 22; j++) {
        for (int i = 0; i < 20; i++) {
            uint8_t px = icon[j][i];
            if (px > 0 && px < 5) {
                gui_draw_pixel(x + i, y + j, pal[px]);
            }
        }
    }
}

static void draw_menu_bar(int active_menu) {
    gui_draw_rect_fill(0, 0, GUI_WIDTH, 20, GUI_COLOR_WHITE);
    gui_draw_line_h(0, 20, GUI_WIDTH, GUI_COLOR_BLACK);

    // Apple / migOS Logo
    gui_draw_char(8, 2, 1, GUI_COLOR_BLACK);
    gui_draw_string(24, 2, "migOS", GUI_COLOR_BLACK);
    gui_draw_string(85, 2, "File", GUI_COLOR_BLACK);
    gui_draw_string(135, 2, "Edit", GUI_COLOR_BLACK);
    gui_draw_string(185, 2, "View", GUI_COLOR_BLACK);
    gui_draw_string(235, 2, "Special", GUI_COLOR_BLACK);
    gui_draw_string(305, 2, "Help", GUI_COLOR_BLACK);

    // Relogio de Uptime no canto superior direito
    char up_str[32];
    sprintf(up_str, "%us", (unsigned int)get_uptime());
    gui_draw_string(GUI_WIDTH - 60, 2, up_str, GUI_COLOR_DARK_GRAY);

    // Destaque do menu ativo selecionado
    if (active_menu == 1) { // Menu migOS
        gui_draw_rect_fill(4, 0, 68, 20, GUI_COLOR_BLACK);
        gui_draw_char(8, 2, 1, GUI_COLOR_WHITE);
        gui_draw_string(24, 2, "migOS", GUI_COLOR_WHITE);
    } else if (active_menu == 2) { // Menu Special
        gui_draw_rect_fill(230, 0, 68, 20, GUI_COLOR_BLACK);
        gui_draw_string(235, 2, "Special", GUI_COLOR_WHITE);
    }
}

static void draw_dropdown_menu(int menu_id, int hover_item) {
    if (menu_id == 1) {
        // Menu "migOS"
        int mx = 4, my = 21, mw = 160, mh = 80;
        gui_draw_rect_fill(mx + 3, my + 3, mw, mh, GUI_COLOR_BLACK);
        gui_draw_rect_fill(mx, my, mw, mh, GUI_COLOR_WHITE);
        gui_draw_rect(mx, my, mw, mh, GUI_COLOR_BLACK);

        const char* items[] = {
            "About migOS...",
            "System Profile",
            "Hardware Info",
            "Exit to Terminal"
        };

        for (int i = 0; i < 4; i++) {
            int iy = my + 4 + (i * 18);
            if (hover_item == i) {
                gui_draw_rect_fill(mx + 2, iy - 2, mw - 4, 18, GUI_COLOR_BLACK);
                gui_draw_string(mx + 8, iy, items[i], GUI_COLOR_WHITE);
            } else {
                gui_draw_string(mx + 8, iy, items[i], GUI_COLOR_BLACK);
            }
        }
    } else if (menu_id == 2) {
        // Menu "Special"
        int mx = 230, my = 21, mw = 170, mh = 80;
        gui_draw_rect_fill(mx + 3, my + 3, mw, mh, GUI_COLOR_BLACK);
        gui_draw_rect_fill(mx, my, mw, mh, GUI_COLOR_WHITE);
        gui_draw_rect(mx, my, mw, mh, GUI_COLOR_BLACK);

        const char* items[] = {
            "Snake Game",
            "Clean Up Desktop",
            "Restart migOS",
            "Shut Down (CLI)"
        };

        for (int i = 0; i < 4; i++) {
            int iy = my + 4 + (i * 18);
            if (hover_item == i) {
                gui_draw_rect_fill(mx + 2, iy - 2, mw - 4, 18, GUI_COLOR_BLACK);
                gui_draw_string(mx + 8, iy, items[i], GUI_COLOR_WHITE);
            } else {
                gui_draw_string(mx + 8, iy, items[i], GUI_COLOR_BLACK);
            }
        }
    }
}

void gui_draw_window(gui_window_t* win) {
    if (!win->is_open) return;

    // Sombra da janela (3px)
    gui_draw_rect_fill(win->x + 3, win->y + 3, win->w, win->h, GUI_COLOR_BLACK);

    // Corpo da janela
    gui_draw_rect_fill(win->x, win->y, win->w, win->h, GUI_COLOR_WHITE);
    gui_draw_rect(win->x, win->y, win->w, win->h, GUI_COLOR_BLACK);

    // Barra de Titulo com Pinstripes
    gui_draw_rect_fill(win->x + 1, win->y + 1, win->w - 2, 19, GUI_COLOR_WHITE);
    for (int line_y = win->y + 3; line_y <= win->y + 17; line_y += 2) {
        gui_draw_line_h(win->x + 24, line_y, win->w - 32, GUI_COLOR_DARK_GRAY);
    }

    // Go-Away Box (Botao Fechar quadrado)
    gui_draw_rect(win->x + 4, win->y + 3, 14, 14, GUI_COLOR_BLACK);
    gui_draw_rect_fill(win->x + 5, win->y + 4, 12, 12, GUI_COLOR_WHITE);

    // Titulo centralizado
    int title_len = (int)strlen(win->title) * 8;
    int title_x = win->x + (win->w / 2) - (title_len / 2);
    gui_draw_rect_fill(title_x - 6, win->y + 2, title_len + 12, 16, GUI_COLOR_WHITE);
    gui_draw_string(title_x, win->y + 2, win->title, GUI_COLOR_BLACK);

    // Linha divisoria da barra de titulo
    gui_draw_line_h(win->x, win->y + 20, win->w, GUI_COLOR_BLACK);
}

static void draw_mouse_cursor(int mx, int my) {
    for (int y = 0; y < 18; y++) {
        for (int x = 0; x < 12; x++) {
            uint8_t px = cursor_sprite[y][x];
            if (px == 1) gui_draw_pixel(mx + x, my + y, GUI_COLOR_BLACK);
            else if (px == 2) gui_draw_pixel(mx + x, my + y, GUI_COLOR_WHITE);
        }
    }
}

void gui_launch_desktop(void) {
    backbuffer = (uint32_t*)kmalloc(GUI_SCREEN_SIZE);
    if (!backbuffer) return;

    bga_init();
    keyboard_set_doom_mode(1);

    // Configura limites do mouse para 640x480 e centraliza o cursor
    mouse_set_bounds(0, 0, GUI_WIDTH - 1, GUI_HEIGHT - 1);
    mouse_set_position(GUI_WIDTH / 2, GUI_HEIGHT / 2);

    // Janela 1: Perfil do Sistema (About)
    gui_window_t win_about = {
        .x = 40, .y = 50, .w = 460, .h = 340,
        .title = "System Profile - About migOS",
        .is_open = 1, .is_dragging = 0,
        .drag_off_x = 0, .drag_off_y = 0
    };

    // Janela 2: migOS HD (Visualizador de arquivos do RAMDisk)
    gui_window_t win_files = {
        .x = 60, .y = 70, .w = 430, .h = 300,
        .title = "migOS HD - 5 itens",
        .is_open = 0, .is_dragging = 0,
        .drag_off_x = 0, .drag_off_y = 0
    };

    int running = 1;
    int active_menu = 0; // 0 = Nenhum, 1 = migOS, 2 = Special
    int hover_menu_item = -1;
    int selected_icon = 0; // 0 = Nenhum, 1 = HD, 2 = Snake, 3 = Term, 4 = Trash
    uint32_t last_click_time = 0;
    int last_click_icon = 0;
    int prev_left_button = 0;

    while (running) {
        // 1. Processa Teclado (ESC ou Q para sair)
        int pressed;
        unsigned char key;
        while (keyboard_get_doom_key(&pressed, &key)) {
            if (pressed && (key == 27 || key == 'q' || key == 'Q')) {
                running = 0;
            }
        }

        // 2. Le Estado do Mouse
        mouse_state_t mouse;
        mouse_get_state(&mouse);

        int mx = mouse.x;
        int my = mouse.y;
        if (mx < 0) mx = 0;
        else if (mx >= GUI_WIDTH) mx = GUI_WIDTH - 1;

        if (my < 0) my = 0;
        else if (my >= GUI_HEIGHT) my = GUI_HEIGHT - 1;

        int click_just_pressed = (mouse.left_button && !prev_left_button);
        uint32_t now = timer_get_ticks();

        // 3. Processamento de Menus Suspensos
        hover_menu_item = -1;
        if (active_menu == 1) { // Menu migOS
            if (mx >= 4 && mx <= 164 && my >= 21 && my <= 101) {
                hover_menu_item = (my - 23) / 18;
                if (hover_menu_item < 0) hover_menu_item = 0;
                if (hover_menu_item > 3) hover_menu_item = 3;
            }
            if (click_just_pressed) {
                if (hover_menu_item == 0 || hover_menu_item == 1) {
                    win_about.is_open = 1;
                    win_about.x = 40; win_about.y = 50;
                } else if (hover_menu_item == 3) {
                    running = 0;
                }
                active_menu = 0;
            }
        } else if (active_menu == 2) { // Menu Special
            if (mx >= 230 && mx <= 400 && my >= 21 && my <= 101) {
                hover_menu_item = (my - 23) / 18;
                if (hover_menu_item < 0) hover_menu_item = 0;
                if (hover_menu_item > 3) hover_menu_item = 3;
            }
            if (click_just_pressed) {
                if (hover_menu_item == 0) { // Snake Game
                    vga_clear();
                    snake_game_main();
                    bga_init();
                    keyboard_set_doom_mode(1);
                    mouse_set_bounds(0, 0, GUI_WIDTH - 1, GUI_HEIGHT - 1);
                    mouse_set_position(GUI_WIDTH / 2, GUI_HEIGHT / 2);
                    active_menu = 0;
                    selected_icon = 0;
                    prev_left_button = 0;
                } else if (hover_menu_item == 2) { // Restart
                    reboot_system();
                } else if (hover_menu_item == 3) { // Shut Down (CLI)
                    running = 0;
                }
                active_menu = 0;
            }
        }

        // Clique na Barra de Menus (Y: 0..20)
        if (click_just_pressed && my <= 20) {
            if (mx >= 0 && mx <= 75) {
                active_menu = (active_menu == 1) ? 0 : 1;
            } else if (mx >= 230 && mx <= 300) {
                active_menu = (active_menu == 2) ? 0 : 2;
            } else {
                active_menu = 0;
            }
        } else if (click_just_pressed && my > 20 && active_menu != 0 && hover_menu_item == -1) {
            active_menu = 0;
        }

        // 4. Processamento de Janelas e Botoes
        if (active_menu == 0) {
            // Janela 1: System Profile
            if (win_about.is_open) {
                if (click_just_pressed) {
                    if (mx >= win_about.x + 4 && mx <= win_about.x + 18 &&
                        my >= win_about.y + 3 && my <= win_about.y + 17) {
                        win_about.is_open = 0;
                    } else if (mx >= win_about.x && mx <= win_about.x + win_about.w &&
                               my >= win_about.y && my <= win_about.y + 20) {
                        win_about.is_dragging = 1;
                        win_about.drag_off_x = mx - win_about.x;
                        win_about.drag_off_y = my - win_about.y;
                    }
                }
            }

            // Janela 2: migOS HD
            if (win_files.is_open) {
                if (click_just_pressed) {
                    if (mx >= win_files.x + 4 && mx <= win_files.x + 18 &&
                        my >= win_files.y + 3 && my <= win_files.y + 17) {
                        win_files.is_open = 0;
                    } else if (mx >= win_files.x && mx <= win_files.x + win_files.w &&
                               my >= win_files.y && my <= win_files.y + 20) {
                        win_files.is_dragging = 1;
                        win_files.drag_off_x = mx - win_files.x;
                        win_files.drag_off_y = my - win_files.y;
                    }
                }
            }

            if (!mouse.left_button) {
                win_about.is_dragging = 0;
                win_files.is_dragging = 0;
            }

            if (win_about.is_dragging) {
                win_about.x = mx - win_about.drag_off_x;
                win_about.y = my - win_about.drag_off_y;
                if (win_about.x < 4) win_about.x = 4;
                if (win_about.y < 22) win_about.y = 22;
            }

            if (win_files.is_dragging) {
                win_files.x = mx - win_files.drag_off_x;
                win_files.y = my - win_files.drag_off_y;
                if (win_files.x < 4) win_files.x = 4;
                if (win_files.y < 22) win_files.y = 22;
            }

            // 5. Processamento de Icones do Desktop (Lado Direito: X=560..635)
            if (click_just_pressed && !win_about.is_dragging && !win_files.is_dragging) {
                int clicked_icon = 0;

                // Icone 1: migOS HD (x: 560..630, y: 30..80)
                if (mx >= 560 && mx <= 630 && my >= 30 && my <= 80) {
                    clicked_icon = 1;
                }
                // Icone 2: Snake (x: 560..630, y: 110..160)
                else if (mx >= 560 && mx <= 630 && my >= 110 && my <= 160) {
                    clicked_icon = 2;
                }
                // Icone 3: Terminal.app (x: 560..630, y: 190..240)
                else if (mx >= 560 && mx <= 630 && my >= 190 && my <= 240) {
                    clicked_icon = 3;
                }
                // Icone 4: Trash (x: 560..630, y: 380..440)
                else if (mx >= 560 && mx <= 630 && my >= 380 && my <= 440) {
                    clicked_icon = 4;
                }

                if (clicked_icon != 0) {
                    selected_icon = clicked_icon;
                    if (last_click_icon == clicked_icon && (now - last_click_time) < 40) {
                        if (clicked_icon == 1) {
                            win_files.is_open = 1;
                            win_files.x = 70; win_files.y = 80;
                        } else if (clicked_icon == 2) {
                            vga_clear();
                            snake_game_main();
                            bga_init();
                            keyboard_set_doom_mode(1);
                            mouse_set_bounds(0, 0, GUI_WIDTH - 1, GUI_HEIGHT - 1);
                            mouse_set_position(GUI_WIDTH / 2, GUI_HEIGHT / 2);
                            active_menu = 0;
                            selected_icon = 0;
                            prev_left_button = 0;
                        } else if (clicked_icon == 3) {
                            running = 0;
                        }
                    }
                    last_click_icon = clicked_icon;
                    last_click_time = now;
                } else if (my > 20 && (!win_about.is_open || mx < win_about.x || mx > win_about.x + win_about.w || my < win_about.y || my > win_about.y + win_about.h)) {
                    selected_icon = 0;
                }
            }
        }

        prev_left_button = mouse.left_button;

        // 6. RENDERIZACAO COMPLETA NO BACKBUFFER 640x480
        draw_desktop_background();

        // Renderiza Icones no Desktop
        // Icone 1: migOS HD
        draw_icon_24x18(580, 35, icon_hd_24x18);
        if (selected_icon == 1) {
            gui_draw_rect_fill(562, 58, 60, 18, GUI_COLOR_BLACK);
            gui_draw_string(566, 60, "migOS HD", GUI_COLOR_WHITE);
        } else {
            gui_draw_string(566, 60, "migOS HD", GUI_COLOR_BLACK);
        }

        // Icone 2: Snake.app
        draw_icon_24x18(580, 115, icon_snake_24x18);
        if (selected_icon == 2) {
            gui_draw_rect_fill(564, 138, 56, 18, GUI_COLOR_BLACK);
            gui_draw_string(568, 140, "Snake", GUI_COLOR_WHITE);
        } else {
            gui_draw_string(568, 140, "Snake", GUI_COLOR_BLACK);
        }

        // Icone 3: Terminal.app
        draw_icon_24x18(580, 195, icon_term_24x18);
        if (selected_icon == 3) {
            gui_draw_rect_fill(558, 218, 68, 18, GUI_COLOR_BLACK);
            gui_draw_string(562, 220, "Terminal", GUI_COLOR_WHITE);
        } else {
            gui_draw_string(562, 220, "Terminal", GUI_COLOR_BLACK);
        }

        // Icone 4: Trash
        draw_icon_trash_20x22(582, 385, icon_trash_20x22);
        if (selected_icon == 4) {
            gui_draw_rect_fill(566, 412, 52, 18, GUI_COLOR_BLACK);
            gui_draw_string(570, 414, "Trash", GUI_COLOR_WHITE);
        } else {
            gui_draw_string(570, 414, "Trash", GUI_COLOR_BLACK);
        }

        // Renderiza Janela de Arquivos (migOS HD) se aberta
        if (win_files.is_open) {
            gui_draw_window(&win_files);
            gui_draw_string_win(&win_files, 12, 28, "Volume de Arquivos RAMDisk (MIGFS):", GUI_COLOR_DARK_GRAY);
            gui_draw_line_h(win_files.x + 12, win_files.y + 48, win_files.w - 24, GUI_COLOR_LIGHT_GRAY);

            size_t fcount = migfs_get_file_count();
            int file_rel_y = 56;
            for (size_t i = 0; i < fcount && i < 10; i++) {
                migfs_file_t* f = migfs_get_file_by_index(i);
                if (f) {
                    draw_icon_24x18(win_files.x + 14, win_files.y + file_rel_y, icon_hd_24x18);
                    gui_draw_string_win(&win_files, 44, file_rel_y + 2, f->name, GUI_COLOR_BLACK);

                    char sz_str[32];
                    sprintf(sz_str, "%u Bytes", (unsigned int)f->size);
                    gui_draw_string_win(&win_files, 220, file_rel_y + 2, sz_str, GUI_COLOR_DARK_GRAY);

                    const char* attr_str = (f->flags & MIGFS_FILE_READONLY) ? "[RO]" : "[RW]";
                    gui_draw_string_win(&win_files, 340, file_rel_y + 2, attr_str, GUI_COLOR_DARK_GRAY);

                    file_rel_y += 24;
                }
            }
        }

        // Renderiza Janela Principal (System Profile) se aberta
        if (win_about.is_open) {
            gui_draw_window(&win_about);

            // Icone e Titulo
            draw_icon_24x18(win_about.x + 14, win_about.y + 28, icon_hd_24x18);
            gui_draw_string_win(&win_about, 44, 30, "migOS System 7 Classic Desktop", GUI_COLOR_BLACK);

            gui_draw_line_h(win_about.x + 12, win_about.y + 52, win_about.w - 24, GUI_COLOR_LIGHT_GRAY);

            // Detalhes Completos do Sistema
            gui_draw_string_win(&win_about, 14, 60, "Sistema Operacional: migOS Kernel v0.5", GUI_COLOR_DARK_GRAY);
            gui_draw_string_win(&win_about, 14, 82, "Arquitetura: x86 IA-32 / Modo Protegido (Ring 0)", GUI_COLOR_DARK_GRAY);

            char mem_str[64];
            sprintf(mem_str, "Memoria Fisica (PMM): %u MB (16,384 Frames 4KB)", (unsigned int)(pmm_get_total_memory() / (1024 * 1024)));
            gui_draw_string_win(&win_about, 14, 104, mem_str, GUI_COLOR_DARK_GRAY);

            sprintf(mem_str, "Heap Dinamico: %u KB Livres de 8 MB", (unsigned int)(kheap_get_free_bytes() / 1024));
            gui_draw_string_win(&win_about, 14, 126, mem_str, GUI_COLOR_DARK_GRAY);

            gui_draw_string_win(&win_about, 14, 148, "Display: 640x480 @ 32-bit True Color (Bochs/QEMU BGA)", GUI_COLOR_DARK_GRAY);
            gui_draw_string_win(&win_about, 14, 170, "Sistema de Arquivos: RAMDisk MIGFS (5 Arquivos)", GUI_COLOR_DARK_GRAY);
            gui_draw_string_win(&win_about, 14, 192, "Mouse: PS/2 IntelliMouse com Roda de Rolagem", GUI_COLOR_DARK_GRAY);

            gui_draw_line_h(win_about.x + 12, win_about.y + 220, win_about.w - 24, GUI_COLOR_LIGHT_GRAY);

            gui_draw_string_win(&win_about, 14, 234, "Arraste a janela ou feche no botao [X]", GUI_COLOR_ACCENT_BLUE);
            gui_draw_string_win(&win_about, 14, 256, "Pressione [ESC] para retornar ao Terminal CLI", GUI_COLOR_DARK_GRAY);
        }

        // Renderiza Barra de Menus Superior
        draw_menu_bar(active_menu);

        // Renderiza Menus Suspensos Abertos
        if (active_menu != 0) {
            draw_dropdown_menu(active_menu, hover_menu_item);
        }

        // Renderiza Cursor do Mouse
        draw_mouse_cursor(mx, my);

        // Envia o frame concluido para o Framebuffer Linear de Hardware (LFB)
        bga_blit(backbuffer);

        sleep(10);
    }

    // Limpeza e restauracao limpa do Terminal CLI
    keyboard_set_doom_mode(0);
    kfree(backbuffer);
    backbuffer = NULL;

    vga_clear();
}
