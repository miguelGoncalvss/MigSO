#ifndef DRIVERS_KEYBOARD_H
#define DRIVERS_KEYBOARD_H

#include <libc/stdint.h>

void keyboard_init(void);
int  keyboard_has_key(void);
void keyboard_clear_key(void);

// Controle de modo do teclado para o DOOM (captura de press e release)
void keyboard_set_doom_mode(int enabled);
int  keyboard_get_doom_key(int* pressed, unsigned char* doom_key);

#endif // DRIVERS_KEYBOARD_H
