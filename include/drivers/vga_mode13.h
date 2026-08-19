#ifndef DRIVERS_VGA_MODE13_H
#define DRIVERS_VGA_MODE13_H

#include <libc/stdint.h>

#define VGA_MODE13_WIDTH    320
#define VGA_MODE13_HEIGHT   200
#define VGA_MODE13_SIZE     (VGA_MODE13_WIDTH * VGA_MODE13_HEIGHT)
#define VGA_MODE13_MEMORY   ((uint8_t*)0xA0000)

// Ativa o Modo Gráfico 13h (320x200 com 256 cores) via portas de hardware VGA
void vga_set_mode_13h(void);

// Restaura o Modo Texto 03h (80x25 caracteres)
void vga_set_mode_text(void);

// Configura uma cor da paleta DAC (R, G, B na faixa 0..63)
void vga_set_palette_color(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

// Configura a paleta completa de 256 cores (768 bytes RGB de 0..63 ou 0..255)
void vga_set_palette_all(const uint8_t* palette_rgb);

// Restaura a paleta padrao de 16 cores do Modo Texto VGA
void vga_restore_default_palette(void);

// Desenha um pixel no frame (x: 0..319, y: 0..199)
void vga_mode13_putpixel(int x, int y, uint8_t color);

// Limpa o framebuffer gráfico com uma cor
void vga_mode13_clear(uint8_t color);

// Copia o buffer de vídeo do jogo (64.000 bytes) diretamente para 0xA0000
void vga_mode13_blit(const uint8_t* buffer);

// Desenha um retangulo preenchido
void vga_mode13_fill_rect(int x, int y, int w, int h, uint8_t color);

// Informa se o modo gráfico 13h está atualmente ativo
int vga_is_mode13_active(void);

#endif // DRIVERS_VGA_MODE13_H
