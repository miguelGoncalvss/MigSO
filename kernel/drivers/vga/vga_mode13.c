#include <drivers/vga_mode13.h>
#include <drivers/vga.h>
#include <gui/font8x8.h>
#include <arch/i386/io.h>
#include <libc/string.h>

#define VGA_MISC_WRITE          0x3C2
#define VGA_SEQ_INDEX           0x3C4
#define VGA_SEQ_DATA            0x3C5
#define VGA_CRTC_INDEX          0x3D4
#define VGA_CRTC_DATA           0x3D5
#define VGA_GC_INDEX            0x3CE
#define VGA_GC_DATA             0x3CF
#define VGA_AC_INDEX            0x3C0
#define VGA_AC_READ             0x3C1
#define VGA_AC_RESET            0x3DA
#define VGA_DAC_WRITE_INDEX     0x3C8
#define VGA_DAC_DATA            0x3C9

static int mode13_active = 0;

// Paleta padrao de 16 cores do Modo Texto VGA (valores DAC em 6-bits 0..63)
static const uint8_t vga_default_palette_16[16][3] = {
    { 0,  0,  0}, // 0: Preto
    { 0,  0, 42}, // 1: Azul
    { 0, 42,  0}, // 2: Verde
    { 0, 42, 42}, // 3: Ciano
    {42,  0,  0}, // 4: Vermelho
    {42,  0, 42}, // 5: Magenta
    {42, 21,  0}, // 6: Marrom
    {42, 42, 42}, // 7: Cinza Claro
    {21, 21, 21}, // 8: Cinza Escuro
    {21, 21, 63}, // 9: Azul Claro
    {21, 63, 21}, // 10: Verde Claro
    {21, 63, 63}, // 11: Ciano Claro
    {63, 21, 21}, // 12: Vermelho Claro
    {63, 21, 63}, // 13: Magenta Claro
    {63, 63, 21}, // 14: Amarelo
    {63, 63, 63}  // 15: Branco
};

// Registradores do Modo 13h (320x200 256 cores)
static const unsigned char mode13h_regs[] = {
    // 0: MISC
    0x63,
    // 1-5: SEQ (5)
    0x03, 0x01, 0x0F, 0x00, 0x0E,
    // 6-30: CRTC (25)
    0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
    0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x9C, 0x8E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3,
    0xFF,
    // 31-39: GC (9)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F,
    0xFF,
    // 40-60: AC (21)
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x41, 0x00, 0x0F, 0x00, 0x00
};

// Registradores do Modo 03h (Texto 80x25 / 400 scanlines)
static const unsigned char mode03h_regs[] = {
    // 0: MISC
    0x67,
    // 1-5: SEQ (5)
    0x03, 0x00, 0x03, 0x00, 0x02,
    // 6-30: CRTC (25)
    0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F,
    0x00, 0x4F, 0x0E, 0x0F, 0x00, 0x00, 0x00, 0x00,
    0x9C, 0x8E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3,
    0xFF,
    // 31-39: GC (9)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00,
    0xFF,
    // 40-60: AC (21)
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x0C, 0x00, 0x0F, 0x00, 0x00
};

static void write_vga_registers(const unsigned char* regs) {
    // 1. Escreve MISC Register
    outb(VGA_MISC_WRITE, regs[0]);

    // 2. Escreve Sequencer (desbloqueia reset sincrono durante mudanca)
    outb(VGA_SEQ_INDEX, 0x00);
    outb(VGA_SEQ_DATA, 0x01);
    for (int i = 1; i < 5; i++) {
        outb(VGA_SEQ_INDEX, (unsigned char)i);
        outb(VGA_SEQ_DATA, regs[1 + i]);
    }
    outb(VGA_SEQ_INDEX, 0x00);
    outb(VGA_SEQ_DATA, 0x03);

    // 3. Desbloqueia escrita dos registradores CRTC 0-7
    outb(VGA_CRTC_INDEX, 0x03);
    outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) | 0x80);
    outb(VGA_CRTC_INDEX, 0x11);
    outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) & ~0x80);

    // 4. Escreve CRTC (25 registradores)
    for (int i = 0; i < 25; i++) {
        outb(VGA_CRTC_INDEX, (unsigned char)i);
        outb(VGA_CRTC_DATA, regs[6 + i]);
    }

    // 5. Escreve Graphics Controller (9 registradores)
    for (int i = 0; i < 9; i++) {
        outb(VGA_GC_INDEX, (unsigned char)i);
        outb(VGA_GC_DATA, regs[31 + i]);
    }

    // 6. Escreve Attribute Controller (21 registradores)
    for (int i = 0; i < 21; i++) {
        inb(VGA_AC_RESET); // Reseta o flip-flop index/data
        outb(VGA_AC_INDEX, (unsigned char)i);
        outb(VGA_AC_INDEX, regs[40 + i]);
    }

    // Reativa a saida de video no Attribute Controller (bit 5 = 1)
    inb(VGA_AC_RESET);
    outb(VGA_AC_INDEX, 0x20);
}

// Restaura os glifos da fonte no Plane 2 da memoria de video VGA
static void vga_load_font_to_plane2(void) {
    // 1. Configura Sequencer para escrita exclusiva no Plane 2 (Fonte de Caracteres)
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x01); // Reset sincrono
    outb(VGA_SEQ_INDEX, 0x02); outb(VGA_SEQ_DATA, 0x04); // Map Mask = Plane 2
    outb(VGA_SEQ_INDEX, 0x04); outb(VGA_SEQ_DATA, 0x06); // Memory Mode = Sequencial (Linear)
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x03); // Fim do Reset

    // 2. Configura Graphics Controller para escrita normal em 0xA0000
    outb(VGA_GC_INDEX, 0x04); outb(VGA_GC_DATA, 0x02); // Read Map Select = Plane 2
    outb(VGA_GC_INDEX, 0x05); outb(VGA_GC_DATA, 0x00); // Mode = Normal 0
    outb(VGA_GC_INDEX, 0x06); outb(VGA_GC_DATA, 0x00); // Miscellaneous = Base 0xA0000 (64 KB)

    // 3. Grava todos os 128 glifos da fonte no Plane 2 (32 bytes por caractere)
    volatile uint8_t* font_mem = (volatile uint8_t*)0xA0000;
    for (int c = 0; c < 128; c++) {
        const uint8_t* glyph = font8x8_basic[c];
        for (int r = 0; r < 8; r++) {
            // Em Modo Texto 80x25 (16 scanlines por caractere), duplica cada linha do 8x8
            font_mem[c * 32 + (r * 2)]     = glyph[r];
            font_mem[c * 32 + (r * 2) + 1] = glyph[r];
        }
        for (int p = 16; p < 32; p++) {
            font_mem[c * 32 + p] = 0x00;
        }
    }

    // 4. Restaura Sequencer e Graphics Controller para Modo Texto (Planes 0 e 1 em 0xB8000)
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x01);
    outb(VGA_SEQ_INDEX, 0x02); outb(VGA_SEQ_DATA, 0x03); // Map Mask = Planes 0 e 1 (Char + Attr)
    outb(VGA_SEQ_INDEX, 0x04); outb(VGA_SEQ_DATA, 0x02); // Memory Mode = Odd/Even
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x03);

    outb(VGA_GC_INDEX, 0x04); outb(VGA_GC_DATA, 0x00);
    outb(VGA_GC_INDEX, 0x05); outb(VGA_GC_DATA, 0x10); // Odd/Even addressing
    outb(VGA_GC_INDEX, 0x06); outb(VGA_GC_DATA, 0x0E); // Memory Map = Base 0xB8000
}

void vga_restore_default_palette(void) {
    outb(VGA_DAC_WRITE_INDEX, 0);
    for (int i = 0; i < 16; i++) {
        outb(VGA_DAC_DATA, vga_default_palette_16[i][0]);
        outb(VGA_DAC_DATA, vga_default_palette_16[i][1]);
        outb(VGA_DAC_DATA, vga_default_palette_16[i][2]);
    }
}

void vga_set_mode_13h(void) {
    write_vga_registers(mode13h_regs);
    mode13_active = 1;
    vga_mode13_clear(0);
}

void vga_set_mode_text(void) {
    write_vga_registers(mode03h_regs);
    vga_load_font_to_plane2();
    vga_restore_default_palette();
    mode13_active = 0;
    vga_init();
    vga_clear();
}

int vga_is_mode13_active(void) {
    return mode13_active;
}

void vga_set_palette_color(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    outb(VGA_DAC_WRITE_INDEX, index);
    if (r > 63 || g > 63 || b > 63) {
        r >>= 2;
        g >>= 2;
        b >>= 2;
    }
    outb(VGA_DAC_DATA, r);
    outb(VGA_DAC_DATA, g);
    outb(VGA_DAC_DATA, b);
}

void vga_set_palette_all(const uint8_t* palette_rgb) {
    if (!palette_rgb) return;
    outb(VGA_DAC_WRITE_INDEX, 0);
    for (int i = 0; i < 256 * 3; i += 3) {
        outb(VGA_DAC_DATA, (uint8_t)(palette_rgb[i] >> 2));
        outb(VGA_DAC_DATA, (uint8_t)(palette_rgb[i + 1] >> 2));
        outb(VGA_DAC_DATA, (uint8_t)(palette_rgb[i + 2] >> 2));
    }
}

void vga_mode13_putpixel(int x, int y, uint8_t color) {
    if (x < 0 || x >= VGA_MODE13_WIDTH || y < 0 || y >= VGA_MODE13_HEIGHT) {
        return;
    }
    VGA_MODE13_MEMORY[y * VGA_MODE13_WIDTH + x] = color;
}

void vga_mode13_clear(uint8_t color) {
    memset(VGA_MODE13_MEMORY, color, VGA_MODE13_SIZE);
}

void vga_mode13_blit(const uint8_t* buffer) {
    if (!buffer) return;
    memcpy(VGA_MODE13_MEMORY, buffer, VGA_MODE13_SIZE);
}

void vga_mode13_fill_rect(int x, int y, int w, int h, uint8_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > VGA_MODE13_WIDTH)  w = VGA_MODE13_WIDTH - x;
    if (y + h > VGA_MODE13_HEIGHT) h = VGA_MODE13_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    for (int row = y; row < y + h; row++) {
        memset(&VGA_MODE13_MEMORY[row * VGA_MODE13_WIDTH + x], color, w);
    }
}
