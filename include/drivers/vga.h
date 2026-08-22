#ifndef DRIVERS_VGA_H
#define DRIVERS_VGA_H

#define VGA_WIDTH 80
#define VGA_HEIGHT 30
#define VGA_MEMORY ((volatile unsigned short*)0xB8000)

enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_YELLOW = 14,
    VGA_COLOR_WHITE = 15,
};

void vga_init(void);
void vga_clear(void);
void vga_set_color(unsigned char fg, unsigned char bg);
void vga_set_cursor(int row, int col);
void vga_get_cursor(int* row, int* col);
void vga_set_cell(int x, int y, char c, unsigned char fg, unsigned char bg);
void vga_putc(char c);
void vga_puts(const char* str);
void vga_scroll(void);
void vga_scroll_history_up(int lines);
void vga_scroll_history_down(int lines);
void vga_scroll_history_reset(void);
int  vga_get_scroll_offset(void);

#endif // DRIVERS_VGA_H
