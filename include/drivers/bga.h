#ifndef DRIVERS_BGA_H
#define DRIVERS_BGA_H

#include <libc/stdint.h>

#define SCREEN_WIDTH        640
#define SCREEN_HEIGHT       480
#define SCREEN_BPP          32
#define SCREEN_PITCH        (SCREEN_WIDTH * 4)
#define SCREEN_SIZE_BYTES   (SCREEN_WIDTH * SCREEN_HEIGHT * 4)

// Inicializa o adaptador grafico Bochs/QEMU BGA em 640x480 a 32-bit True Color
int bga_init(void);

// Informa se o adaptador BGA esta disponivel
int bga_is_available(void);

// Obtem o endereco linear do Framebuffer (LFB)
uint32_t bga_get_lfb_address(void);

// Retorna o ponteiro para a memoria de video mapeada
uint32_t* bga_get_framebuffer(void);

// Configura a resolucao e profundidade de cor (ex: 640, 480, 32)
void bga_set_video_mode(uint16_t width, uint16_t height, uint16_t bpp);

// Desativa o modo BGA e retorna ao modo VGA padrao
void bga_disable(void);

// Desenha um pixel diretamente no Framebuffer de Hardware
void bga_putpixel(int x, int y, uint32_t color);

// Limpa o Framebuffer de Hardware com uma cor ARGB
void bga_clear(uint32_t color);

// Transfere um buffer de 640x480x4 bytes para o Framebuffer de Hardware (Blit)
void bga_blit(const uint32_t* buffer);

#endif // DRIVERS_BGA_H
