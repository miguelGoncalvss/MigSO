#ifndef DRIVERS_KEYBOARD_H
#define DRIVERS_KEYBOARD_H

#include <libc/stdint.h>

// Definicoes de teclas especiais e de controle
#define KEY_RIGHT_ARROW  0xae
#define KEY_LEFT_ARROW   0xac
#define KEY_UP_ARROW     0xad
#define KEY_DOWN_ARROW   0xaf
#define KEY_ESCAPE       27
#define KEY_ENTER        13
#define KEY_TAB          9
#define KEY_BACKSPACE    127
#define KEY_DELETE       0x7f
#define KEY_HOME         0xe0
#define KEY_END          0xe1
#define KEY_PAGEUP       0xe2
#define KEY_PAGEDOWN     0xe3
#define KEY_INSERT       0xe4

#define KEY_F1           (0x80+0x3b)
#define KEY_F2           (0x80+0x3c)
#define KEY_F3           (0x80+0x3d)
#define KEY_F4           (0x80+0x3e)
#define KEY_F5           (0x80+0x3f)
#define KEY_F6           (0x80+0x40)
#define KEY_F7           (0x80+0x41)
#define KEY_F8           (0x80+0x42)
#define KEY_F9           (0x80+0x43)
#define KEY_F10          (0x80+0x44)
#define KEY_F11          (0x80+0x57)
#define KEY_F12          (0x80+0x58)

#define KEY_RSHIFT       (0x80+0x36)
#define KEY_LSHIFT       (0x80+0x2a)
#define KEY_RCTRL        (0x80+0x1d)
#define KEY_LCTRL        (0x80+0x1d)
#define KEY_RALT         (0x80+0x38)
#define KEY_LALT         (0x80+0x38)

// Estrutura de evento de teclado
typedef struct {
    int pressed;            // 1 se pressionada, 0 se solta
    unsigned char key;      // Codigo ASCII ou chave especial
    int ctrl;               // Estado da tecla Ctrl
    int shift;              // Estado da tecla Shift
    int alt;                // Estado da tecla Alt
} key_event_t;

void keyboard_init(void);
int  keyboard_has_key(void);
void keyboard_clear_key(void);

// Modos de captura direta para DOOM, Jogos, Editor CLI e GUI
void keyboard_set_doom_mode(int enabled);
int  keyboard_get_doom_key(int* pressed, unsigned char* doom_key);
int  keyboard_get_event(key_event_t* event);

// Consultas de modificadores
int  keyboard_is_ctrl_down(void);
int  keyboard_is_shift_down(void);
int  keyboard_is_alt_down(void);

#endif // DRIVERS_KEYBOARD_H
