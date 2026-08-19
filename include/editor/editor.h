#ifndef EDITOR_EDITOR_H
#define EDITOR_EDITOR_H

#include <libc/stdint.h>
#include <libc/string.h>

// Ponto de entrada do Editor de Texto em modo console (VGA 80x30 / 80x25)
void editor_open_cli(const char* filename);

#endif // EDITOR_EDITOR_H
