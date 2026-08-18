#include <drivers/vga.h>
#include <arch/i386/io.h>

static int cursor_row = 0;
static int cursor_col = 0;
static unsigned char current_color = 0x07;

static inline unsigned short vga_entry(unsigned char ch, unsigned char color) {
    return (unsigned short)ch | ((unsigned short)color << 8);
}

static void update_hardware_cursor(void) {
    unsigned short position = (cursor_row * VGA_WIDTH) + cursor_col;

    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((position >> 8) & 0xFF));

    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(position & 0xFF));
}

void vga_clear(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry(' ', current_color);
        }
    }
    cursor_row = 0;
    cursor_col = 0;
    update_hardware_cursor();
}

void vga_init(void) {
    current_color = ((VGA_COLOR_BLACK & 0x0F) << 4) | (VGA_COLOR_LIGHT_GREEN & 0x0F);
    vga_clear(); // Limpa totalmente o lixo visual deixado pela BIOS
}

void vga_set_color(unsigned char fg, unsigned char bg) {
    current_color = ((bg & 0x0F) << 4) | (fg & 0x0F);
}

void vga_set_cursor(int row, int col) {
    cursor_row = row;
    cursor_col = col;
    update_hardware_cursor();
}

void vga_scroll(void) {
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[y * VGA_WIDTH + x] = VGA_MEMORY[(y + 1) * VGA_WIDTH + x];
        }
    }

    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', current_color);
    }

    cursor_row = VGA_HEIGHT - 1;
    update_hardware_cursor();
}

void vga_putc(char c) {
    if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(' ', current_color);
        }
    } else if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else {
        VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(c, current_color);
        cursor_col++;
    }

    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
    }

    if (cursor_row >= VGA_HEIGHT) {
        vga_scroll();
    }

    update_hardware_cursor();
}

void vga_puts(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        vga_putc(str[i]);
    }
}
