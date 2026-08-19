#ifndef EMULATOR_GAMEBOY_H
#define EMULATOR_GAMEBOY_H

#include <stdint.h>
#include <stddef.h>

/**
 * Inicia a execucao do emulador de Game Boy para a ROM indicada.
 * rom_filename: Nome do arquivo .gb no sistema de arquivos MIGFS (ex: "pokemon.gb")
 * Retorna 0 em caso de sucesso ou codigo de erro negativo.
 */
int gameboy_launch(const char* rom_filename);

/**
 * Salva a SRAM do cartucho (Save Game) no disco ATA persistente (MIGFS).
 */
void gameboy_sync_save(void);

/**
 * Exibe informacoes e status do cartucho carregado.
 */
void gameboy_print_cart_info(const char* rom_filename);

#endif // EMULATOR_GAMEBOY_H
