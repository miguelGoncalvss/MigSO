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
#include <interpreter/txt_interp.h>
#include <games/snake.h>
#include <emulator/gameboy.h>
#include <libc/stdio.h>
#include <libc/stdlib.h>
#include "font8x8.h"

static uint32_t* backbuffer = NULL;

// Sprite 12x18 do cursor classico System 7
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

// Icone Pasta / Folder 24x18 (Estilo Mac OS System 7 Classic)
static const uint8_t icon_folder_24x18[18][24] = {
    {0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,1,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {0,1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,0},
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

// Icone TextEdit / Documento 24x18
static const uint8_t icon_edit_24x18[18][24] = {
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0},
    {0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,0,0,0,0},
    {1,2,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0},
    {0,1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,0,0,0,0},
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0},
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

// Icone Pokemon / Pokeball 24x18 (Estilo Mac OS System 7 Classic)
static const uint8_t icon_pokemon_24x18[18][24] = {
    {0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0},
    {0,0,0,0,0,1,1,4,4,4,4,4,4,4,4,4,4,1,1,0,0,0,0,0},
    {0,0,0,0,1,4,4,4,4,4,4,4,4,4,4,4,4,4,4,1,0,0,0,0},
    {0,0,0,1,4,4,2,2,4,4,4,4,4,4,4,4,4,4,4,4,1,0,0,0},
    {0,0,1,4,4,2,2,2,4,4,4,4,4,4,4,4,4,4,4,4,4,1,0,0},
    {0,1,4,4,4,4,2,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,1,0},
    {0,1,4,4,4,4,4,4,4,4,1,1,1,1,4,4,4,4,4,4,4,4,1,0},
    {1,1,1,1,1,1,1,1,1,1,1,2,2,1,1,1,1,1,1,1,1,1,1,1},
    {1,4,4,4,4,4,4,4,1,2,2,1,1,2,2,1,4,4,4,4,4,4,4,1},
    {1,2,2,2,2,2,2,2,1,2,2,1,1,2,2,1,2,2,2,2,2,2,2,1},
    {1,1,1,1,1,1,1,1,1,1,1,2,2,1,1,1,1,1,1,1,1,1,1,1},
    {0,1,2,2,2,2,2,2,2,2,1,1,1,1,2,2,2,2,2,2,2,2,1,0},
    {0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {0,0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0},
    {0,0,0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0},
    {0,0,0,0,1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,0,0,0,0},
    {0,0,0,0,0,1,1,3,3,3,3,3,3,3,3,3,3,1,1,0,0,0,0,0},
    {0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0}
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

// ============================================================
// Primitivas Graficas
// ============================================================

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

void gui_draw_inset_frame(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    gui_draw_rect_fill(x, y, w, h, GUI_COLOR_WHITE);
    gui_draw_line_h(x, y, w, GUI_COLOR_DARK_GRAY);
    gui_draw_line_v(x, y, h, GUI_COLOR_DARK_GRAY);
    gui_draw_line_h(x, y + h - 1, w, GUI_COLOR_MID_GRAY);
    gui_draw_line_v(x + w - 1, y, h, GUI_COLOR_MID_GRAY);
}

void gui_draw_button(int x, int y, int w, int h, const char* text, int is_pressed) {
    if (w <= 0 || h <= 0) return;
    if (is_pressed) {
        gui_draw_rect_fill(x, y, w, h, GUI_COLOR_BLACK);
        int tlen = (int)strlen(text) * 8;
        int tx = x + (w - tlen) / 2;
        int ty = y + (h - 14) / 2;
        gui_draw_string(tx, ty, text, GUI_COLOR_WHITE);
    } else {
        gui_draw_rect_fill(x, y, w, h, GUI_COLOR_LIGHT_GRAY);
        gui_draw_rect(x, y, w, h, GUI_COLOR_BLACK);
        gui_draw_line_h(x + 1, y + 1, w - 2, GUI_COLOR_WHITE);
        gui_draw_line_v(x + 1, y + 1, h - 2, GUI_COLOR_WHITE);
        gui_draw_line_h(x + 1, y + h - 2, w - 2, GUI_COLOR_MID_GRAY);
        gui_draw_line_v(x + w - 2, y + 1, h - 2, GUI_COLOR_MID_GRAY);
        int tlen = (int)strlen(text) * 8;
        int tx = x + (w - tlen) / 2;
        int ty = y + (h - 14) / 2;
        gui_draw_string(tx, ty, text, GUI_COLOR_BLACK);
    }
}

void gui_draw_char(int x, int y, char c, uint32_t fg) {
    unsigned char uc = (unsigned char)c;
    if (uc > 127) uc = ' ';
    const uint8_t* glyph = font8x8_basic[uc];
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
    unsigned char uc = (unsigned char)c;
    if (uc > 127) uc = ' ';
    const uint8_t* glyph = font8x8_basic[uc];
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

static void draw_icon_folder_24x18(int x, int y, const uint8_t icon[18][24]) {
    uint32_t pal[5] = {0, GUI_COLOR_BLACK, 0x00FFCC00, 0x00CC9900, GUI_COLOR_DARK_GRAY};
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
    } else if (active_menu == 3) { // Menu File
        gui_draw_rect_fill(80, 0, 48, 20, GUI_COLOR_BLACK);
        gui_draw_string(85, 2, "File", GUI_COLOR_WHITE);
    }
}

static void draw_dropdown_menu(int menu_id, int hover_item) {
    if (menu_id == 1) {
        // Menu "migOS"
        int mx = 4, my = 21, mw = 165, mh = 80;
        gui_draw_rect_fill(mx + 3, my + 3, mw, mh, GUI_COLOR_BLACK);
        gui_draw_rect_fill(mx, my, mw, mh, GUI_COLOR_WHITE);
        gui_draw_rect(mx, my, mw, mh, GUI_COLOR_BLACK);

        const char* items[] = {
            "About migOS...",
            "System Profile",
            "Text Editor (GUI)",
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
        int mx = 230, my = 21, mw = 175, mh = 98;
        gui_draw_rect_fill(mx + 3, my + 3, mw, mh, GUI_COLOR_BLACK);
        gui_draw_rect_fill(mx, my, mw, mh, GUI_COLOR_WHITE);
        gui_draw_rect(mx, my, mw, mh, GUI_COLOR_BLACK);

        const char* items[] = {
            "Text Editor",
            "Game Boy (migBoy)",
            "Snake Game",
            "Restart migOS",
            "Shut Down (CLI)"
        };

        for (int i = 0; i < 5; i++) {
            int iy = my + 4 + (i * 18);
            if (hover_item == i) {
                gui_draw_rect_fill(mx + 2, iy - 2, mw - 4, 18, GUI_COLOR_BLACK);
                gui_draw_string(mx + 8, iy, items[i], GUI_COLOR_WHITE);
            } else {
                gui_draw_string(mx + 8, iy, items[i], GUI_COLOR_BLACK);
            }
        }
    } else if (menu_id == 3) {
        // Menu "File"
        int mx = 80, my = 21, mw = 160, mh = 80;
        gui_draw_rect_fill(mx + 3, my + 3, mw, mh, GUI_COLOR_BLACK);
        gui_draw_rect_fill(mx, my, mw, mh, GUI_COLOR_WHITE);
        gui_draw_rect(mx, my, mw, mh, GUI_COLOR_BLACK);

        const char* items[] = {
            "New Document",
            "Open TextEdit",
            "Save Active File",
            "Close Window"
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

// ============================================================
// Buffer e Estado do Editor de Texto GUI (TextEdit)
// ============================================================
#define GUI_EDIT_MAX_CHARS 8192
static char gui_edit_buf[GUI_EDIT_MAX_CHARS];
static int gui_edit_len = 0;
static int gui_cursor_pos = 0;
static char gui_edit_filename[MIGFS_MAX_FILENAME] = "readme.txt";
static int gui_edit_dirty = 0;
static char gui_script_output[2048];

static void gui_load_file_to_editor(const char* filename) {
    strncpy(gui_edit_filename, filename, MIGFS_MAX_FILENAME - 1);
    gui_edit_filename[MIGFS_MAX_FILENAME - 1] = '\0';

    migfs_file_t* f = migfs_open(filename);
    if (f && f->data) {
        size_t to_copy = f->size;
        if (to_copy >= GUI_EDIT_MAX_CHARS - 1) to_copy = GUI_EDIT_MAX_CHARS - 2;
        memcpy(gui_edit_buf, f->data, to_copy);
        gui_edit_buf[to_copy] = '\0';
        gui_edit_len = (int)to_copy;
    } else {
        gui_edit_buf[0] = '\0';
        gui_edit_len = 0;
    }
    gui_cursor_pos = 0;
    gui_edit_dirty = 0;
}

static void gui_save_editor_file(void) {
    migfs_write(gui_edit_filename, gui_edit_buf, (size_t)gui_edit_len);
    gui_edit_dirty = 0;
}

// ============================================================
// Gerenciador de Janelas (Z-Order & Renderizacao)
// ============================================================
#define WIN_ID_ABOUT      1
#define WIN_ID_FILES      2
#define WIN_ID_EDITOR     3
#define WIN_ID_SCRIPT_OUT 4

static int z_order[4] = { WIN_ID_ABOUT, WIN_ID_FILES, WIN_ID_EDITOR, WIN_ID_SCRIPT_OUT };

static void bring_window_to_front(int win_id) {
    int idx = -1;
    for (int i = 0; i < 4; i++) {
        if (z_order[i] == win_id) {
            idx = i;
            break;
        }
    }
    if (idx != -1 && idx < 3) {
        for (int i = idx; i < 3; i++) {
            z_order[i] = z_order[i + 1];
        }
        z_order[3] = win_id;
    }
}

static void render_win_about(gui_window_t* win) {
    if (!win->is_open) return;
    gui_draw_window(win);

    draw_icon_24x18(win->x + 14, win->y + 28, icon_hd_24x18);
    gui_draw_string_win(win, 44, 30, "migOS System 7 Classic Desktop", GUI_COLOR_BLACK);
    gui_draw_line_h(win->x + 12, win->y + 52, win->w - 24, GUI_COLOR_LIGHT_GRAY);

    gui_draw_string_win(win, 14, 60, "Sistema Operacional: migOS Kernel v0.5", GUI_COLOR_DARK_GRAY);
    gui_draw_string_win(win, 14, 82, "Arquitetura: x86 IA-32 / Modo Protegido (Ring 0)", GUI_COLOR_DARK_GRAY);

    char mem_str[64];
    sprintf(mem_str, "Memoria Fisica (PMM): %u MB (16,384 Frames 4KB)", (unsigned int)(pmm_get_total_memory() / (1024 * 1024)));
    gui_draw_string_win(win, 14, 104, mem_str, GUI_COLOR_DARK_GRAY);

    sprintf(mem_str, "Heap Dinamico: %u KB Livres de 8 MB", (unsigned int)(kheap_get_free_bytes() / 1024));
    gui_draw_string_win(win, 14, 126, mem_str, GUI_COLOR_DARK_GRAY);

    gui_draw_string_win(win, 14, 148, "Display: 640x480 @ 32-bit True Color (BGA LFB)", GUI_COLOR_DARK_GRAY);
    gui_draw_string_win(win, 14, 170, "Disco: MIGFS Persistente em Disco ATA Primario", GUI_COLOR_DARK_GRAY);
    gui_draw_string_win(win, 14, 192, "Mouse: PS/2 IntelliMouse com Roda de Rolagem", GUI_COLOR_DARK_GRAY);

    gui_draw_line_h(win->x + 12, win->y + 220, win->w - 24, GUI_COLOR_LIGHT_GRAY);
    gui_draw_string_win(win, 14, 234, "Arraste a janela ou feche no botao [X]", GUI_COLOR_ACCENT_BLUE);
    gui_draw_string_win(win, 14, 256, "Pressione [ESC] para retornar ao Terminal CLI", GUI_COLOR_DARK_GRAY);
}

// ============================================================
// Estado do Gerenciador de Arquivos GUI (migOS HD / Finder)
// ============================================================
#define DIALOG_NONE      0
#define DIALOG_MKDIR     1
#define DIALOG_MOVE      2
#define DIALOG_COPY      3
#define DIALOG_DELETE    4
#define DIALOG_NEW_FILE  5

static int dialog_open = 0;
static int dialog_type = DIALOG_NONE;
static char dialog_title[48] = "";
static char dialog_prompt[64] = "";
static char dialog_input[MIGFS_MAX_FILENAME] = "";
static int dialog_input_pos = 0;
static char dialog_target_item[MIGFS_MAX_FILENAME] = "";

static char gui_files_cwd[MIGFS_MAX_FILENAME] = "/";
static int gui_files_selected = -1;
static char gui_files_title[64] = "migOS HD - /";

static void execute_dialog_action(void) {
    if (!dialog_open) return;

    if (dialog_type == DIALOG_MKDIR) {
        if (dialog_input[0] != '\0') {
            char full_p[MIGFS_MAX_FILENAME];
            migfs_path_combine(gui_files_cwd, dialog_input, full_p, sizeof(full_p));
            migfs_mkdir(full_p);
        }
    } else if (dialog_type == DIALOG_NEW_FILE) {
        if (dialog_input[0] != '\0') {
            char full_p[MIGFS_MAX_FILENAME];
            migfs_path_combine(gui_files_cwd, dialog_input, full_p, sizeof(full_p));
            migfs_create(full_p, "", 0, 0);
            gui_load_file_to_editor(full_p);
        }
    } else if (dialog_type == DIALOG_MOVE) {
        if (dialog_input[0] != '\0' && dialog_target_item[0] != '\0') {
            char full_p[MIGFS_MAX_FILENAME];
            migfs_path_combine(gui_files_cwd, dialog_input, full_p, sizeof(full_p));
            migfs_move(dialog_target_item, full_p);
        }
    } else if (dialog_type == DIALOG_COPY) {
        if (dialog_input[0] != '\0' && dialog_target_item[0] != '\0') {
            char full_p[MIGFS_MAX_FILENAME];
            migfs_path_combine(gui_files_cwd, dialog_input, full_p, sizeof(full_p));
            migfs_copy(dialog_target_item, full_p);
        }
    } else if (dialog_type == DIALOG_DELETE) {
        if (dialog_target_item[0] != '\0') {
            if (migfs_is_dir(dialog_target_item)) {
                migfs_rmdir(dialog_target_item);
            } else {
                migfs_delete(dialog_target_item);
            }
        }
    }

    dialog_open = 0;
    dialog_type = DIALOG_NONE;
    gui_files_selected = -1;
}

static void render_dialog_modal(void) {
    if (!dialog_open) return;

    int dx = 150, dy = 140, dw = 340, dh = 155;

    // Sombra do modal (4px)
    gui_draw_rect_fill(dx + 4, dy + 4, dw, dh, GUI_COLOR_BLACK);

    // Corpo
    gui_draw_rect_fill(dx, dy, dw, dh, GUI_COLOR_WHITE);
    gui_draw_rect(dx, dy, dw, dh, GUI_COLOR_BLACK);
    gui_draw_rect(dx + 2, dy + 2, dw - 4, dh - 4, GUI_COLOR_BLACK);

    // Barra de Titulo
    gui_draw_rect_fill(dx + 3, dy + 3, dw - 6, 20, GUI_COLOR_TITLE_BLUE);
    gui_draw_string(dx + 12, dy + 5, dialog_title, GUI_COLOR_WHITE);

    // Prompt
    gui_draw_string(dx + 16, dy + 35, dialog_prompt, GUI_COLOR_BLACK);

    if (dialog_type == DIALOG_DELETE) {
        gui_draw_string(dx + 16, dy + 60, "Esta acao removera o item permanentemente.", GUI_COLOR_LIGHT_RED);
        gui_draw_button(dx + dw - 185, dy + dh - 34, 80, 22, "Excluir", 0);
        gui_draw_button(dx + dw - 95, dy + dh - 34, 75, 22, "Cancelar", 0);
    } else {
        gui_draw_inset_frame(dx + 16, dy + 60, dw - 32, 26);
        gui_draw_string_clipped(dx + 22, dy + 66, dialog_input, GUI_COLOR_BLACK, dx + dw - 24);

        int cur_x = dx + 22 + (dialog_input_pos * 8);
        if (cur_x < dx + dw - 24) {
            gui_draw_line_v(cur_x, dy + 64, 16, GUI_COLOR_BLACK);
            gui_draw_line_v(cur_x + 1, dy + 64, 16, GUI_COLOR_BLACK);
        }

        gui_draw_button(dx + dw - 185, dy + dh - 34, 80, 22, "Confirmar", 0);
        gui_draw_button(dx + dw - 95, dy + dh - 34, 75, 22, "Cancelar", 0);
    }
}

static void render_win_files(gui_window_t* win) {
    if (!win->is_open) return;

    snprintf(gui_files_title, sizeof(gui_files_title), "migOS HD - %s", gui_files_cwd);
    win->title = gui_files_title;
    gui_draw_window(win);

    // Toolbar de botoes do Gerenciador de Arquivos
    gui_draw_button(win->x + 10, win->y + 24, 60, 18, "< Voltar", 0);
    gui_draw_button(win->x + 74, win->y + 24, 62, 18, "+ Pasta", 0);
    gui_draw_button(win->x + 140, win->y + 24, 52, 18, "+ Arq", 0);
    gui_draw_button(win->x + 196, win->y + 24, 50, 18, "Mover", 0);
    gui_draw_button(win->x + 250, win->y + 24, 54, 18, "Copiar", 0);
    gui_draw_button(win->x + 308, win->y + 24, 58, 18, "Excluir", 0);

    // Exibe Pasta Atual
    char cur_path_str[64];
    snprintf(cur_path_str, sizeof(cur_path_str), "Pasta: %s", gui_files_cwd);
    gui_draw_string_win(win, 12, 46, cur_path_str, GUI_COLOR_DARK_GRAY);

    // Frame da lista de arquivos e pastas
    int list_x = win->x + 10;
    int list_y = win->y + 60;
    int list_w = win->w - 20;
    int list_h = win->h - 88;
    gui_draw_inset_frame(list_x, list_y, list_w, list_h);

    static migfs_dir_item_t items[32];
    size_t count = 0;
    migfs_get_dir_items(gui_files_cwd, items, 32, &count);

    int max_vis = list_h / 24;
    for (size_t i = 0; i < count && i < (size_t)max_vis; i++) {
        int item_y = list_y + 4 + ((int)i * 24);

        if (gui_files_selected == (int)i) {
            gui_draw_rect_fill(list_x + 2, item_y - 2, list_w - 4, 22, GUI_COLOR_TITLE_BLUE);
        }

        uint32_t text_color = (gui_files_selected == (int)i) ? GUI_COLOR_WHITE : GUI_COLOR_BLACK;
        uint32_t meta_color = (gui_files_selected == (int)i) ? GUI_COLOR_WHITE : GUI_COLOR_DARK_GRAY;

        if (items[i].is_dir) {
            draw_icon_folder_24x18(list_x + 6, item_y, icon_folder_24x18);
            gui_draw_string_clipped(list_x + 36, item_y + 2, items[i].name, text_color, list_x + 220);
            gui_draw_string_clipped(list_x + 240, item_y + 2, "<PASTA>", meta_color, list_x + 330);
        } else if (strstr(items[i].name, ".gb")) {
            draw_icon_24x18(list_x + 6, item_y, icon_pokemon_24x18);
            gui_draw_string_clipped(list_x + 36, item_y + 2, items[i].name, text_color, list_x + 220);
            char sz_str[32];
            sprintf(sz_str, "%u B", (unsigned int)items[i].size);
            gui_draw_string_clipped(list_x + 240, item_y + 2, sz_str, meta_color, list_x + 330);
        } else {
            draw_icon_24x18(list_x + 6, item_y, icon_edit_24x18);
            gui_draw_string_clipped(list_x + 36, item_y + 2, items[i].name, text_color, list_x + 220);
            char sz_str[32];
            sprintf(sz_str, "%u B", (unsigned int)items[i].size);
            gui_draw_string_clipped(list_x + 240, item_y + 2, sz_str, meta_color, list_x + 330);
        }

        const char* attr = (items[i].flags & MIGFS_FILE_READONLY) ? "[RO]" : "[RW]";
        gui_draw_string_clipped(list_x + 350, item_y + 2, attr, meta_color, list_x + list_w - 6);
    }

    char footer_str[80];
    sprintf(footer_str, "%d item(ns) na pasta | Duplo clique para abrir", (int)count);
    gui_draw_string_win(win, 12, win->h - 20, footer_str, GUI_COLOR_DARK_GRAY);
}

static void render_win_editor(gui_window_t* win) {
    if (!win->is_open) return;

    char ed_title[64];
    sprintf(ed_title, "TextEdit - %s%s", gui_edit_filename, gui_edit_dirty ? " *" : "");
    win->title = ed_title;
    gui_draw_window(win);

    // Botoes da Barra de Ferramentas
    gui_draw_button(win->x + 12, win->y + 25, 48, 18, "Novo", 0);
    gui_draw_button(win->x + 64, win->y + 25, 48, 18, "Abrir", 0);
    gui_draw_button(win->x + 116, win->y + 25, 54, 18, "Salvar", 0);
    gui_draw_button(win->x + 174, win->y + 25, 70, 18, "Executar", 0);
    gui_draw_button(win->x + 248, win->y + 25, 54, 18, "Limpar", 0);

    // Caixinha de nome do arquivo
    gui_draw_rect(win->x + 308, win->y + 25, 145, 18, GUI_COLOR_BLACK);
    gui_draw_rect_fill(win->x + 309, win->y + 26, 143, 16, GUI_COLOR_LIGHT_GRAY);
    gui_draw_string_clipped(win->x + 314, win->y + 27, gui_edit_filename, GUI_COLOR_BLACK, win->x + 448);

    // Inset Frame do Texto
    int can_x = win->x + 12;
    int can_y = win->y + 48;
    int can_w = win->w - 24;
    int can_h = win->h - 78;
    gui_draw_inset_frame(can_x, can_y, can_w, can_h);

    // Renderizacao do Texto no Frame
    int render_l = 0, render_c = 0;
    int cursor_drawn_x = -1, cursor_drawn_y = -1;

    int max_visible_lines = can_h / 16;
    int max_visible_cols = can_w / 8;

    for (int p = 0; p <= gui_edit_len; p++) {
        if (p == gui_cursor_pos) {
            cursor_drawn_x = can_x + 6 + (render_c * 8);
            cursor_drawn_y = can_y + 4 + (render_l * 16);
        }

        if (p == gui_edit_len) break;

        char c = gui_edit_buf[p];
        if (c == '\n') {
            render_l++;
            render_c = 0;
        } else if (c == '\r') {
            // ignora
        } else {
            if (render_l < max_visible_lines && render_c < max_visible_cols) {
                int cx = can_x + 6 + (render_c * 8);
                int cy = can_y + 4 + (render_l * 16);
                gui_draw_char_clipped(cx, cy, c, GUI_COLOR_BLACK, can_x + 2, can_x + can_w - 4, can_y + 2, can_y + can_h - 4);
            }
            render_c++;
        }
    }

    // Desenha cursor vertical |
    if (cursor_drawn_x >= can_x + 4 && cursor_drawn_x <= can_x + can_w - 4 &&
        cursor_drawn_y >= can_y + 2 && cursor_drawn_y <= can_y + can_h - 14) {
        gui_draw_line_v(cursor_drawn_x, cursor_drawn_y, 14, GUI_COLOR_BLACK);
        gui_draw_line_v(cursor_drawn_x + 1, cursor_drawn_y, 14, GUI_COLOR_BLACK);
    }

    // Linha de Status inferior do Editor
    char ed_status[80];
    sprintf(ed_status, "%d B | %s", gui_edit_len, gui_edit_dirty ? "[Modificado]" : "[Salvo no Disco ATA]");
    gui_draw_string_win(win, 14, win->h - 22, ed_status, GUI_COLOR_DARK_GRAY);
}

static void render_win_script_out(gui_window_t* win) {
    if (!win->is_open) return;
    gui_draw_window(win);

    gui_draw_string_win(win, 14, 28, "Log de Execucao do Interpretador .txt:", GUI_COLOR_DARK_GRAY);
    gui_draw_inset_frame(win->x + 12, win->y + 48, win->w - 24, win->h - 80);

    // Renderiza linhas de saida do script
    int out_l = 0, out_c = 0;
    for (int p = 0; gui_script_output[p] != '\0' && out_l < 11; p++) {
        char oc = gui_script_output[p];
        if (oc == '\n') {
            out_l++;
            out_c = 0;
        } else {
            if (out_c < 48) {
                int ox = win->x + 16 + (out_c * 8);
                int oy = win->y + 54 + (out_l * 16);
                gui_draw_char_clipped(ox, oy, oc, GUI_COLOR_BLACK, win->x + 14, win->x + win->w - 14, win->y + 50, win->y + win->h - 36);
            }
            out_c++;
        }
    }

    gui_draw_button(win->x + win->w - 85, win->y + win->h - 26, 70, 20, "Fechar", 0);
}

// Ponto de entrada do Ambiente Grafico (Mac OS System 7 Classic Desktop)
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
        .x = 40, .y = 45, .w = 460, .h = 340,
        .title = "System Profile - About migOS",
        .is_open = 1, .is_dragging = 0,
        .drag_off_x = 0, .drag_off_y = 0
    };

    // Janela 2: migOS HD (Gerenciador de Arquivos e Diretorios)
    gui_window_t win_files = {
        .x = 60, .y = 65, .w = 460, .h = 330,
        .title = "migOS HD - /",
        .is_open = 0, .is_dragging = 0,
        .drag_off_x = 0, .drag_off_y = 0
    };

    // Janela 3: TextEdit (Editor de Texto com Persistencia em Disco e Script Runner)
    gui_window_t win_editor = {
        .x = 75, .y = 48, .w = 465, .h = 355,
        .title = "TextEdit - readme.txt",
        .is_open = 0, .is_dragging = 0,
        .drag_off_x = 0, .drag_off_y = 0
    };

    // Janela 4: Script Output Window
    gui_window_t win_script_out = {
        .x = 100, .y = 80, .w = 430, .h = 280,
        .title = "Script Runner - Resultado",
        .is_open = 0, .is_dragging = 0,
        .drag_off_x = 0, .drag_off_y = 0
    };

    gui_load_file_to_editor("readme.txt");

    int running = 1;
    int active_menu = 0; // 0 = Nenhum, 1 = migOS, 2 = Special, 3 = File
    int hover_menu_item = -1;
    int selected_icon = 0; // 0 = Nenhum, 1 = HD, 2 = TextEdit, 3 = Snake, 4 = Term, 5 = Trash
    uint32_t last_click_time = 0;
    int last_click_icon = 0;
    uint32_t last_file_click_time = 0;
    int last_file_click_idx = -1;
    int prev_left_button = 0;

    while (running) {
        // 1. Processa Teclado
        int pressed;
        unsigned char key;
        while (keyboard_get_doom_key(&pressed, &key)) {
            if (!pressed) continue;

            if (dialog_open) {
                if (key == KEY_ESCAPE) {
                    dialog_open = 0;
                    dialog_type = DIALOG_NONE;
                } else if (key == KEY_ENTER) {
                    execute_dialog_action();
                } else if (key == KEY_BACKSPACE) {
                    if (dialog_input_pos > 0) {
                        dialog_input[--dialog_input_pos] = '\0';
                    }
                } else if (key >= 32 && key <= 126) {
                    if (dialog_input_pos < MIGFS_MAX_FILENAME - 2) {
                        dialog_input[dialog_input_pos++] = (char)key;
                        dialog_input[dialog_input_pos] = '\0';
                    }
                }
                continue;
            }

            if (key == KEY_ESCAPE) {
                if (win_script_out.is_open) {
                    win_script_out.is_open = 0;
                } else if (win_editor.is_open) {
                    win_editor.is_open = 0;
                } else if (win_files.is_open) {
                    win_files.is_open = 0;
                } else if (win_about.is_open) {
                    win_about.is_open = 0;
                } else {
                    running = 0;
                }
            } else if (win_editor.is_open) {
                // Entrada no Editor de Texto
                if (key == KEY_BACKSPACE) {
                    if (gui_cursor_pos > 0) {
                        for (int i = gui_cursor_pos - 1; i < gui_edit_len; i++) {
                            gui_edit_buf[i] = gui_edit_buf[i + 1];
                        }
                        gui_edit_len--;
                        gui_cursor_pos--;
                        gui_edit_dirty = 1;
                    }
                } else if (key == KEY_DELETE) {
                    if (gui_cursor_pos < gui_edit_len) {
                        for (int i = gui_cursor_pos; i < gui_edit_len; i++) {
                            gui_edit_buf[i] = gui_edit_buf[i + 1];
                        }
                        gui_edit_len--;
                        gui_edit_dirty = 1;
                    }
                } else if (key == KEY_ENTER) {
                    if (gui_edit_len < GUI_EDIT_MAX_CHARS - 2) {
                        for (int i = gui_edit_len; i > gui_cursor_pos; i--) {
                            gui_edit_buf[i] = gui_edit_buf[i - 1];
                        }
                        gui_edit_buf[gui_cursor_pos++] = '\n';
                        gui_edit_len++;
                        gui_edit_buf[gui_edit_len] = '\0';
                        gui_edit_dirty = 1;
                    }
                } else if (key == KEY_LEFT_ARROW) {
                    if (gui_cursor_pos > 0) gui_cursor_pos--;
                } else if (key == KEY_RIGHT_ARROW) {
                    if (gui_cursor_pos < gui_edit_len) gui_cursor_pos++;
                } else if (key == KEY_HOME) {
                    while (gui_cursor_pos > 0 && gui_edit_buf[gui_cursor_pos - 1] != '\n') {
                        gui_cursor_pos--;
                    }
                } else if (key == KEY_END) {
                    while (gui_cursor_pos < gui_edit_len && gui_edit_buf[gui_cursor_pos] != '\n') {
                        gui_cursor_pos++;
                    }
                } else if (key == KEY_TAB) {
                    for (int t = 0; t < 4 && gui_edit_len < GUI_EDIT_MAX_CHARS - 2; t++) {
                        for (int i = gui_edit_len; i > gui_cursor_pos; i--) {
                            gui_edit_buf[i] = gui_edit_buf[i - 1];
                        }
                        gui_edit_buf[gui_cursor_pos++] = ' ';
                        gui_edit_len++;
                        gui_edit_buf[gui_edit_len] = '\0';
                        gui_edit_dirty = 1;
                    }
                } else if (key >= 32 && key <= 126) {
                    if (gui_edit_len < GUI_EDIT_MAX_CHARS - 2) {
                        for (int i = gui_edit_len; i > gui_cursor_pos; i--) {
                            gui_edit_buf[i] = gui_edit_buf[i - 1];
                        }
                        gui_edit_buf[gui_cursor_pos++] = (char)key;
                        gui_edit_len++;
                        gui_edit_buf[gui_edit_len] = '\0';
                        gui_edit_dirty = 1;
                    }
                }
            } else if (key == 'q' || key == 'Q') {
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
            if (mx >= 4 && mx <= 168 && my >= 21 && my <= 101) {
                hover_menu_item = (my - 23) / 18;
                if (hover_menu_item < 0) hover_menu_item = 0;
                if (hover_menu_item > 3) hover_menu_item = 3;
            }
            if (click_just_pressed) {
                if (hover_menu_item == 0 || hover_menu_item == 1) {
                    win_about.is_open = 1;
                    win_about.x = 40; win_about.y = 45;
                    bring_window_to_front(WIN_ID_ABOUT);
                } else if (hover_menu_item == 2) {
                    win_editor.is_open = 1;
                    bring_window_to_front(WIN_ID_EDITOR);
                } else if (hover_menu_item == 3) {
                    running = 0;
                }
                active_menu = 0;
            }
        } else if (active_menu == 2) { // Menu Special
            if (mx >= 230 && mx <= 405 && my >= 21 && my <= 119) {
                hover_menu_item = (my - 23) / 18;
                if (hover_menu_item < 0) hover_menu_item = 0;
                if (hover_menu_item > 4) hover_menu_item = 4;
            }
            if (click_just_pressed) {
                if (hover_menu_item == 0) { // Text Editor
                    win_editor.is_open = 1;
                    bring_window_to_front(WIN_ID_EDITOR);
                } else if (hover_menu_item == 1) { // Game Boy (migBoy)
                    vga_clear();
                    const char* gb_rom = migfs_exists("pokemon.gb") ? "pokemon.gb" : (migfs_exists("game.gb") ? "game.gb" : "pokemon.gb");
                    gameboy_launch(gb_rom);
                    bga_init();
                    keyboard_set_doom_mode(1);
                    mouse_set_bounds(0, 0, GUI_WIDTH - 1, GUI_HEIGHT - 1);
                    mouse_set_position(GUI_WIDTH / 2, GUI_HEIGHT / 2);
                    active_menu = 0;
                    selected_icon = 0;
                    prev_left_button = 0;
                } else if (hover_menu_item == 2) { // Snake Game
                    vga_clear();
                    snake_game_main();
                    bga_init();
                    keyboard_set_doom_mode(1);
                    mouse_set_bounds(0, 0, GUI_WIDTH - 1, GUI_HEIGHT - 1);
                    mouse_set_position(GUI_WIDTH / 2, GUI_HEIGHT / 2);
                    active_menu = 0;
                    selected_icon = 0;
                    prev_left_button = 0;
                } else if (hover_menu_item == 3) { // Restart
                    reboot_system();
                } else if (hover_menu_item == 4) { // Shut Down
                    running = 0;
                }
                active_menu = 0;
            }
        } else if (active_menu == 3) { // Menu File
            if (mx >= 80 && mx <= 240 && my >= 21 && my <= 101) {
                hover_menu_item = (my - 23) / 18;
                if (hover_menu_item < 0) hover_menu_item = 0;
                if (hover_menu_item > 3) hover_menu_item = 3;
            }
            if (click_just_pressed) {
                if (hover_menu_item == 0) { // New Document
                    gui_edit_buf[0] = '\0';
                    gui_edit_len = 0;
                    gui_cursor_pos = 0;
                    strcpy(gui_edit_filename, "novo.txt");
                    gui_edit_dirty = 1;
                    win_editor.is_open = 1;
                    bring_window_to_front(WIN_ID_EDITOR);
                } else if (hover_menu_item == 1) { // Open TextEdit
                    win_editor.is_open = 1;
                    bring_window_to_front(WIN_ID_EDITOR);
                } else if (hover_menu_item == 2) { // Save File
                    if (win_editor.is_open) gui_save_editor_file();
                } else if (hover_menu_item == 3) { // Close Window
                    if (win_script_out.is_open) win_script_out.is_open = 0;
                    else if (win_editor.is_open) win_editor.is_open = 0;
                    else if (win_files.is_open) win_files.is_open = 0;
                    else if (win_about.is_open) win_about.is_open = 0;
                }
                active_menu = 0;
            }
        }

        // Clique na Barra de Menus Superior (Y: 0..20)
        if (click_just_pressed && my <= 20) {
            if (mx >= 0 && mx <= 75) {
                active_menu = (active_menu == 1) ? 0 : 1;
            } else if (mx >= 80 && mx <= 130) {
                active_menu = (active_menu == 3) ? 0 : 3;
            } else if (mx >= 230 && mx <= 300) {
                active_menu = (active_menu == 2) ? 0 : 2;
            } else {
                active_menu = 0;
            }
        } else if (click_just_pressed && my > 20 && active_menu != 0 && hover_menu_item == -1) {
            active_menu = 0;
        }

        // 4. Processamento de Janelas, Dialogos e Botoes
        if (dialog_open) {
            int dx = 150, dy = 140, dw = 340, dh = 155;
            if (click_just_pressed) {
                // Botao Confirmar / Excluir
                if (mx >= dx + dw - 185 && mx <= dx + dw - 105 && my >= dy + dh - 34 && my <= dy + dh - 12) {
                    execute_dialog_action();
                }
                // Botao Cancelar
                else if (mx >= dx + dw - 95 && mx <= dx + dw - 20 && my >= dy + dh - 34 && my <= dy + dh - 12) {
                    dialog_open = 0;
                    dialog_type = DIALOG_NONE;
                }
            }
        } else if (active_menu == 0) {
            int click_handled = 0;

            if (click_just_pressed) {
                // Percorre as janelas da frente para tras (Z-Order)
                for (int zi = 3; zi >= 0; zi--) {
                    int wid = z_order[zi];

                    if (wid == WIN_ID_SCRIPT_OUT && win_script_out.is_open) {
                        if (mx >= win_script_out.x && mx <= win_script_out.x + win_script_out.w &&
                            my >= win_script_out.y && my <= win_script_out.y + win_script_out.h) {
                            
                            bring_window_to_front(WIN_ID_SCRIPT_OUT);
                            click_handled = 1;

                            if (mx >= win_script_out.x + 4 && mx <= win_script_out.x + 18 &&
                                my >= win_script_out.y + 3 && my <= win_script_out.y + 17) {
                                win_script_out.is_open = 0;
                            } else if (my <= win_script_out.y + 20) {
                                win_script_out.is_dragging = 1;
                                win_script_out.drag_off_x = mx - win_script_out.x;
                                win_script_out.drag_off_y = my - win_script_out.y;
                            } else if (mx >= win_script_out.x + win_script_out.w - 85 && mx <= win_script_out.x + win_script_out.w - 15 &&
                                       my >= win_script_out.y + win_script_out.h - 26 && my <= win_script_out.y + win_script_out.h - 8) {
                                win_script_out.is_open = 0;
                            }
                            break;
                        }
                    } else if (wid == WIN_ID_EDITOR && win_editor.is_open) {
                        if (mx >= win_editor.x && mx <= win_editor.x + win_editor.w &&
                            my >= win_editor.y && my <= win_editor.y + win_editor.h) {
                            
                            bring_window_to_front(WIN_ID_EDITOR);
                            click_handled = 1;

                            if (mx >= win_editor.x + 4 && mx <= win_editor.x + 18 &&
                                my >= win_editor.y + 3 && my <= win_editor.y + 17) {
                                win_editor.is_open = 0;
                            } else if (my <= win_editor.y + 20) {
                                win_editor.is_dragging = 1;
                                win_editor.drag_off_x = mx - win_editor.x;
                                win_editor.drag_off_y = my - win_editor.y;
                            }
                            // Botoes da Barra de Ferramentas
                            else if (my >= win_editor.y + 25 && my <= win_editor.y + 44) {
                                if (mx >= win_editor.x + 12 && mx <= win_editor.x + 60) {
                                    gui_edit_buf[0] = '\0';
                                    gui_edit_len = 0;
                                    gui_cursor_pos = 0;
                                    strcpy(gui_edit_filename, "novo.txt");
                                    gui_edit_dirty = 1;
                                } else if (mx >= win_editor.x + 64 && mx <= win_editor.x + 112) {
                                    static int open_idx = 0;
                                    size_t total_f = migfs_get_file_count();
                                    if (total_f > 0) {
                                        open_idx = (open_idx + 1) % total_f;
                                        migfs_file_t* mf = migfs_get_file_by_index(open_idx);
                                        if (mf) gui_load_file_to_editor(mf->name);
                                    }
                                } else if (mx >= win_editor.x + 116 && mx <= win_editor.x + 170) {
                                    gui_save_editor_file();
                                } else if (mx >= win_editor.x + 174 && mx <= win_editor.x + 244) {
                                    gui_save_editor_file();
                                    script_run_buffer_capture(gui_edit_buf, gui_script_output, sizeof(gui_script_output));
                                    win_script_out.is_open = 1;
                                    win_script_out.x = win_editor.x + 20;
                                    win_script_out.y = win_editor.y + 30;
                                    bring_window_to_front(WIN_ID_SCRIPT_OUT);
                                } else if (mx >= win_editor.x + 248 && mx <= win_editor.x + 300) {
                                    gui_edit_buf[0] = '\0';
                                    gui_edit_len = 0;
                                    gui_cursor_pos = 0;
                                    gui_edit_dirty = 1;
                                }
                            }
                            // Clique no canvas de texto para posicionar o cursor
                            else if (mx >= win_editor.x + 12 && mx <= win_editor.x + win_editor.w - 12 &&
                                     my >= win_editor.y + 48 && my <= win_editor.y + win_editor.h - 30) {
                                int click_line = (my - (win_editor.y + 54)) / 16;
                                int click_col = (mx - (win_editor.x + 18)) / 8;
                                if (click_line < 0) click_line = 0;
                                if (click_col < 0) click_col = 0;

                                int cur_l = 0, p = 0;
                                while (p < gui_edit_len && cur_l < click_line) {
                                    if (gui_edit_buf[p] == '\n') cur_l++;
                                    p++;
                                }
                                int cur_c = 0;
                                while (p < gui_edit_len && gui_edit_buf[p] != '\n' && cur_c < click_col) {
                                    p++;
                                    cur_c++;
                                }
                                gui_cursor_pos = p;
                            }
                            break;
                        }
                    } else if (wid == WIN_ID_FILES && win_files.is_open) {
                        if (mx >= win_files.x && mx <= win_files.x + win_files.w &&
                            my >= win_files.y && my <= win_files.y + win_files.h) {
                            
                            bring_window_to_front(WIN_ID_FILES);
                            click_handled = 1;

                            if (mx >= win_files.x + 4 && mx <= win_files.x + 18 &&
                                my >= win_files.y + 3 && my <= win_files.y + 17) {
                                win_files.is_open = 0;
                            } else if (my <= win_files.y + 20) {
                                win_files.is_dragging = 1;
                                win_files.drag_off_x = mx - win_files.x;
                                win_files.drag_off_y = my - win_files.y;
                            }
                            // Botoes da Barra de Ferramentas do Gerenciador de Arquivos
                            else if (my >= win_files.y + 24 && my <= win_files.y + 44) {
                                // < Voltar
                                if (mx >= win_files.x + 10 && mx <= win_files.x + 70) {
                                    migfs_get_parent_dir(gui_files_cwd, gui_files_cwd, sizeof(gui_files_cwd));
                                    gui_files_selected = -1;
                                }
                                // + Pasta
                                else if (mx >= win_files.x + 74 && mx <= win_files.x + 136) {
                                    dialog_open = 1;
                                    dialog_type = DIALOG_MKDIR;
                                    strcpy(dialog_title, "Nova Pasta");
                                    strcpy(dialog_prompt, "Digite o nome da nova pasta:");
                                    strcpy(dialog_input, "pasta");
                                    dialog_input_pos = 5;
                                    dialog_target_item[0] = '\0';
                                }
                                // + Arq
                                else if (mx >= win_files.x + 140 && mx <= win_files.x + 192) {
                                    dialog_open = 1;
                                    dialog_type = DIALOG_NEW_FILE;
                                    strcpy(dialog_title, "Novo Arquivo");
                                    strcpy(dialog_prompt, "Digite o nome do novo arquivo:");
                                    strcpy(dialog_input, "novo.txt");
                                    dialog_input_pos = 8;
                                    dialog_target_item[0] = '\0';
                                }
                                // Mover / Renomear
                                else if (mx >= win_files.x + 196 && mx <= win_files.x + 246) {
                                    migfs_dir_item_t items[32];
                                    size_t count = 0;
                                    migfs_get_dir_items(gui_files_cwd, items, 32, &count);
                                    if (gui_files_selected >= 0 && (size_t)gui_files_selected < count) {
                                        dialog_open = 1;
                                        dialog_type = DIALOG_MOVE;
                                        strcpy(dialog_title, "Mover / Renomear");
                                        strcpy(dialog_prompt, "Digite o novo nome ou caminho:");
                                        strncpy(dialog_input, items[gui_files_selected].name, sizeof(dialog_input) - 1);
                                        dialog_input_pos = strlen(dialog_input);
                                        strncpy(dialog_target_item, items[gui_files_selected].full_path, sizeof(dialog_target_item) - 1);
                                    }
                                }
                                // Copiar
                                else if (mx >= win_files.x + 250 && mx <= win_files.x + 304) {
                                    migfs_dir_item_t items[32];
                                    size_t count = 0;
                                    migfs_get_dir_items(gui_files_cwd, items, 32, &count);
                                    if (gui_files_selected >= 0 && (size_t)gui_files_selected < count) {
                                        dialog_open = 1;
                                        dialog_type = DIALOG_COPY;
                                        strcpy(dialog_title, "Copiar Arquivo");
                                        strcpy(dialog_prompt, "Digite o nome da copia / destino:");
                                        snprintf(dialog_input, sizeof(dialog_input), "copia_%s", items[gui_files_selected].name);
                                        dialog_input_pos = strlen(dialog_input);
                                        strncpy(dialog_target_item, items[gui_files_selected].full_path, sizeof(dialog_target_item) - 1);
                                    }
                                }
                                // Excluir
                                else if (mx >= win_files.x + 308 && mx <= win_files.x + 366) {
                                    static migfs_dir_item_t items[32];
                                    size_t count = 0;
                                    migfs_get_dir_items(gui_files_cwd, items, 32, &count);
                                    if (gui_files_selected >= 0 && (size_t)gui_files_selected < count) {
                                        dialog_open = 1;
                                        dialog_type = DIALOG_DELETE;
                                        strcpy(dialog_title, "Excluir Item");
                                        snprintf(dialog_prompt, sizeof(dialog_prompt), "Excluir '%s'?", items[gui_files_selected].name);
                                        dialog_input[0] = '\0';
                                        dialog_input_pos = 0;
                                        strncpy(dialog_target_item, items[gui_files_selected].full_path, sizeof(dialog_target_item) - 1);
                                    }
                                }
                            }
                            // Clique na lista de arquivos/pastas
                            else if (mx >= win_files.x + 10 && mx <= win_files.x + win_files.w - 10 &&
                                     my >= win_files.y + 60 && my <= win_files.y + win_files.h - 28) {
                                static migfs_dir_item_t items[32];
                                size_t count = 0;
                                migfs_get_dir_items(gui_files_cwd, items, 32, &count);

                                int clicked_idx = (my - (win_files.y + 64)) / 24;
                                if (clicked_idx >= 0 && (size_t)clicked_idx < count) {
                                    if (last_file_click_idx == clicked_idx && (now - last_file_click_time) < 55) {
                                        // Duplo clique bem-sucedido!
                                        if (items[clicked_idx].is_dir) {
                                            if (strcmp(items[clicked_idx].name, "..") == 0) {
                                                migfs_get_parent_dir(gui_files_cwd, gui_files_cwd, sizeof(gui_files_cwd));
                                            } else {
                                                strncpy(gui_files_cwd, items[clicked_idx].full_path, sizeof(gui_files_cwd) - 1);
                                                gui_files_cwd[sizeof(gui_files_cwd) - 1] = '\0';
                                            }
                                            gui_files_selected = -1;
                                            last_file_click_idx = -1;
                                            last_file_click_time = 0;
                                        } else if (strstr(items[clicked_idx].name, ".gb")) {
                                            vga_clear();
                                            gameboy_launch(items[clicked_idx].full_path);
                                            bga_init();
                                            keyboard_set_doom_mode(1);
                                            mouse_set_bounds(0, 0, GUI_WIDTH - 1, GUI_HEIGHT - 1);
                                            mouse_set_position(GUI_WIDTH / 2, GUI_HEIGHT / 2);
                                            active_menu = 0;
                                            selected_icon = 0;
                                            prev_left_button = 0;
                                            last_file_click_idx = -1;
                                            last_file_click_time = 0;
                                        } else {
                                            gui_load_file_to_editor(items[clicked_idx].full_path);
                                            win_editor.is_open = 1;
                                            bring_window_to_front(WIN_ID_EDITOR);
                                            last_file_click_idx = -1;
                                            last_file_click_time = 0;
                                        }
                                    } else {
                                        gui_files_selected = clicked_idx;
                                        last_file_click_idx = clicked_idx;
                                        last_file_click_time = now;
                                    }
                                }
                            }
                            break;
                        }
                    } else if (wid == WIN_ID_ABOUT && win_about.is_open) {
                        if (mx >= win_about.x && mx <= win_about.x + win_about.w &&
                            my >= win_about.y && my <= win_about.y + win_about.h) {
                            
                            bring_window_to_front(WIN_ID_ABOUT);
                            click_handled = 1;

                            if (mx >= win_about.x + 4 && mx <= win_about.x + 18 &&
                                my >= win_about.y + 3 && my <= win_about.y + 17) {
                                win_about.is_open = 0;
                            } else if (my <= win_about.y + 20) {
                                win_about.is_dragging = 1;
                                win_about.drag_off_x = mx - win_about.x;
                                win_about.drag_off_y = my - win_about.y;
                            }
                            break;
                        }
                    }
                }
            }

            if (!mouse.left_button) {
                win_about.is_dragging = 0;
                win_files.is_dragging = 0;
                win_editor.is_dragging = 0;
                win_script_out.is_dragging = 0;
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

            if (win_editor.is_dragging) {
                win_editor.x = mx - win_editor.drag_off_x;
                win_editor.y = my - win_editor.drag_off_y;
                if (win_editor.x < 4) win_editor.x = 4;
                if (win_editor.y < 22) win_editor.y = 22;
            }

            if (win_script_out.is_dragging) {
                win_script_out.x = mx - win_script_out.drag_off_x;
                win_script_out.y = my - win_script_out.drag_off_y;
                if (win_script_out.x < 4) win_script_out.x = 4;
                if (win_script_out.y < 22) win_script_out.y = 22;
            }

            // 5. Processamento de Icones do Desktop (apenas se o clique nao foi absorvido por nenhuma janela)
            if (click_just_pressed && !click_handled &&
                !win_about.is_dragging && !win_files.is_dragging && !win_editor.is_dragging && !win_script_out.is_dragging) {
                
                int clicked_icon = 0;

                // Icone 1: migOS HD (x: 560..630, y: 28..78)
                if (mx >= 560 && mx <= 630 && my >= 28 && my <= 78) {
                    clicked_icon = 1;
                }
                // Icone 2: TextEdit (x: 560..630, y: 94..144)
                else if (mx >= 560 && mx <= 630 && my >= 94 && my <= 144) {
                    clicked_icon = 2;
                }
                // Icone 3: Pokemon.app (x: 560..630, y: 160..210)
                else if (mx >= 560 && mx <= 630 && my >= 160 && my <= 210) {
                    clicked_icon = 3;
                }
                // Icone 4: Snake (x: 560..630, y: 226..276)
                else if (mx >= 560 && mx <= 630 && my >= 226 && my <= 276) {
                    clicked_icon = 4;
                }
                // Icone 5: Terminal.app (x: 560..630, y: 292..342)
                else if (mx >= 560 && mx <= 630 && my >= 292 && my <= 342) {
                    clicked_icon = 5;
                }
                // Icone 6: Trash (x: 560..630, y: 380..440)
                else if (mx >= 560 && mx <= 630 && my >= 380 && my <= 440) {
                    clicked_icon = 6;
                }

                if (clicked_icon != 0) {
                    selected_icon = clicked_icon;
                    if (last_click_icon == clicked_icon && (now - last_click_time) < 40) {
                        if (clicked_icon == 1) {
                            win_files.is_open = 1;
                            win_files.x = 60; win_files.y = 65;
                            bring_window_to_front(WIN_ID_FILES);
                        } else if (clicked_icon == 2) {
                            win_editor.is_open = 1;
                            win_editor.x = 75; win_editor.y = 48;
                            bring_window_to_front(WIN_ID_EDITOR);
                        } else if (clicked_icon == 3) {
                            vga_clear();
                            const char* gb_rom = migfs_exists("PokemonRed.gb") ? "PokemonRed.gb" : (migfs_exists("pokemon.gb") ? "pokemon.gb" : "PokemonRed.gb");
                            gameboy_launch(gb_rom);
                            bga_init();
                            keyboard_set_doom_mode(1);
                            mouse_set_bounds(0, 0, GUI_WIDTH - 1, GUI_HEIGHT - 1);
                            mouse_set_position(GUI_WIDTH / 2, GUI_HEIGHT / 2);
                            active_menu = 0;
                            selected_icon = 0;
                            prev_left_button = 0;
                        } else if (clicked_icon == 4) {
                            vga_clear();
                            snake_game_main();
                            bga_init();
                            keyboard_set_doom_mode(1);
                            mouse_set_bounds(0, 0, GUI_WIDTH - 1, GUI_HEIGHT - 1);
                            mouse_set_position(GUI_WIDTH / 2, GUI_HEIGHT / 2);
                            active_menu = 0;
                            selected_icon = 0;
                            prev_left_button = 0;
                        } else if (clicked_icon == 5) {
                            running = 0;
                        }
                    }
                    last_click_icon = clicked_icon;
                    last_click_time = now;
                } else if (my > 20) {
                    selected_icon = 0;
                }
            }
        }

        prev_left_button = mouse.left_button;

        // 6. RENDERIZACAO COMPLETA NO BACKBUFFER 640x480
        draw_desktop_background();

        // Renderiza Icones no Desktop
        // Icone 1: migOS HD
        draw_icon_24x18(580, 32, icon_hd_24x18);
        if (selected_icon == 1) {
            gui_draw_rect_fill(562, 54, 60, 18, GUI_COLOR_BLACK);
            gui_draw_string(566, 56, "migOS HD", GUI_COLOR_WHITE);
        } else {
            gui_draw_string(566, 56, "migOS HD", GUI_COLOR_BLACK);
        }

        // Icone 2: TextEdit.app
        draw_icon_24x18(580, 98, icon_edit_24x18);
        if (selected_icon == 2) {
            gui_draw_rect_fill(560, 120, 64, 18, GUI_COLOR_BLACK);
            gui_draw_string(564, 122, "TextEdit", GUI_COLOR_WHITE);
        } else {
            gui_draw_string(564, 122, "TextEdit", GUI_COLOR_BLACK);
        }

        // Icone 3: Pokemon.app
        draw_icon_24x18(580, 164, icon_pokemon_24x18);
        if (selected_icon == 3) {
            gui_draw_rect_fill(560, 186, 64, 18, GUI_COLOR_BLACK);
            gui_draw_string(564, 188, "Pokemon", GUI_COLOR_WHITE);
        } else {
            gui_draw_string(564, 188, "Pokemon", GUI_COLOR_BLACK);
        }

        // Icone 4: Snake.app
        draw_icon_24x18(580, 230, icon_snake_24x18);
        if (selected_icon == 4) {
            gui_draw_rect_fill(564, 252, 56, 18, GUI_COLOR_BLACK);
            gui_draw_string(568, 254, "Snake", GUI_COLOR_WHITE);
        } else {
            gui_draw_string(568, 254, "Snake", GUI_COLOR_BLACK);
        }

        // Icone 5: Terminal.app
        draw_icon_24x18(580, 296, icon_term_24x18);
        if (selected_icon == 5) {
            gui_draw_rect_fill(558, 318, 68, 18, GUI_COLOR_BLACK);
            gui_draw_string(562, 320, "Terminal", GUI_COLOR_WHITE);
        } else {
            gui_draw_string(562, 320, "Terminal", GUI_COLOR_BLACK);
        }

        // Icone 6: Trash
        draw_icon_trash_20x22(582, 385, icon_trash_20x22);
        if (selected_icon == 6) {
            gui_draw_rect_fill(566, 412, 52, 18, GUI_COLOR_BLACK);
            gui_draw_string(570, 414, "Trash", GUI_COLOR_WHITE);
        } else {
            gui_draw_string(570, 414, "Trash", GUI_COLOR_BLACK);
        }

        // Renderiza Janelas na ordem do Z-Order (do fundo para a frente)
        for (int zi = 0; zi < 4; zi++) {
            int wid = z_order[zi];
            if (wid == WIN_ID_ABOUT) {
                render_win_about(&win_about);
            } else if (wid == WIN_ID_FILES) {
                render_win_files(&win_files);
            } else if (wid == WIN_ID_EDITOR) {
                render_win_editor(&win_editor);
            } else if (wid == WIN_ID_SCRIPT_OUT) {
                render_win_script_out(&win_script_out);
            }
        }

        // Renderiza Barra de Menus Superior
        draw_menu_bar(active_menu);

        // Renderiza Menus Suspensos Abertos
        if (active_menu != 0) {
            draw_dropdown_menu(active_menu, hover_menu_item);
        }

        // Renderiza Caixa de Dialogo Modal se aberta
        if (dialog_open) {
            render_dialog_modal();
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
