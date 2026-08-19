#ifndef DOOM_GENERIC_H
#define DOOM_GENERIC_H

#include <libc/stdint.h>

#define DOOMGENERIC_RESX 320
#define DOOMGENERIC_RESY 200

extern uint8_t* DG_ScreenBuffer;

// 5 Funcoes fundamentais exigidas pelo doomgeneric
void     DG_Init(void);
void     DG_DrawFrame(void);
void     DG_SleepMs(uint32_t ms);
uint32_t DG_GetTicksMs(void);
int      DG_GetKey(int* pressed, unsigned char* doomKey);

// Funcao auxiliar para atualizacao da paleta do DOOM no hardware VGA DAC
void     DG_SetPalette(const uint8_t* palette);

// Teclas especiais mapeadas do DOOM
#define KEY_RIGHTARROW  0xae
#define KEY_LEFTARROW   0xac
#define KEY_UPARROW     0xad
#define KEY_DOWNARROW   0xaf
#define KEY_ESCAPE      27
#define KEY_ENTER       13
#define KEY_TAB         9
#define KEY_F1          (0x80+0x3b)
#define KEY_F2          (0x80+0x3c)
#define KEY_F3          (0x80+0x3d)
#define KEY_F4          (0x80+0x3e)
#define KEY_F5          (0x80+0x3f)
#define KEY_F6          (0x80+0x40)
#define KEY_F7          (0x80+0x41)
#define KEY_F8          (0x80+0x42)
#define KEY_F9          (0x80+0x43)
#define KEY_F10         (0x80+0x44)
#define KEY_F11         (0x80+0x57)
#define KEY_F12         (0x80+0x58)
#define KEY_BACKSPACE   127
#define KEY_PAUSE       0xff
#define KEY_EQUALS      0x3d
#define KEY_MINUS       0x2d
#define KEY_RSHIFT      (0x80+0x36)
#define KEY_RCTRL       (0x80+0x1d)
#define KEY_RALT        (0x80+0x38)
#define KEY_LALT        KEY_RALT

// Ponto de entrada do executavel DOOM no migOS
void doom_main(int argc, char** argv);

#endif // DOOM_GENERIC_H
