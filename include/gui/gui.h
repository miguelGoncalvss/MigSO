#ifndef GUI_H
#define GUI_H

#include <libc/stdint.h>
#include <libc/string.h>

#define GUI_WIDTH       640
#define GUI_HEIGHT      480
#define GUI_SCREEN_SIZE (GUI_WIDTH * GUI_HEIGHT * sizeof(uint32_t))

// Cores 32-bit True Color (0x00RRGGBB) no padrao Mac OS System 7 Platinum
#define GUI_COLOR_BLACK         0x00000000
#define GUI_COLOR_WHITE         0x00FFFFFF
#define GUI_COLOR_LIGHT_GRAY    0x00DDDDDD
#define GUI_COLOR_MID_GRAY      0x00AAAAAA
#define GUI_COLOR_DARK_GRAY     0x00555555
#define GUI_COLOR_ACCENT_BLUE   0x00000080
#define GUI_COLOR_TITLE_BLUE    0x00336699
#define GUI_COLOR_YELLOW        0x00FFFF00
#define GUI_COLOR_LIGHT_BLUE    0x005555FF
#define GUI_COLOR_LIGHT_GREEN   0x0000AA00
#define GUI_COLOR_LIGHT_RED     0x00CC0000

typedef struct {
    int x, y;
    int w, h;
    const char* title;
    int is_open;
    int is_dragging;
    int drag_off_x;
    int drag_off_y;
} gui_window_t;

// Ponto de entrada do ambiente grafico (bloqueia ate o usuario fechar/ESC)
void gui_launch_desktop(void);

// Primitivas de renderizacao no backbuffer (32-bit ARGB)
void gui_draw_pixel(int x, int y, uint32_t color);
void gui_draw_rect(int x, int y, int w, int h, uint32_t color);
void gui_draw_rect_fill(int x, int y, int w, int h, uint32_t color);
void gui_draw_line_h(int x, int y, int w, uint32_t color);
void gui_draw_line_v(int x, int y, int h, uint32_t color);
void gui_draw_char(int x, int y, char c, uint32_t fg);
void gui_draw_char_clipped(int x, int y, char c, uint32_t fg, int min_x, int max_x, int min_y, int max_y);
void gui_draw_string(int x, int y, const char* str, uint32_t fg);
void gui_draw_string_clipped(int x, int y, const char* str, uint32_t fg, int max_x);
void gui_draw_string_win(gui_window_t* win, int rel_x, int rel_y, const char* str, uint32_t fg);
void gui_draw_window(gui_window_t* win);

#endif // GUI_H
