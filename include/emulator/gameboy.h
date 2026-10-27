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

/**
 * Funcoes do Emulador Game Boy para execucao em Janela no Ambiente Grafico (GUI)
 */
int  gameboy_gui_load_rom(const char* rom_filename);
void gameboy_gui_step_frame(void);
void gameboy_gui_handle_key(int pressed, unsigned char key);
const uint32_t* gameboy_gui_get_screen(void);
int  gameboy_gui_is_loaded(void);
const char* gameboy_gui_get_rom_title(void);
void gameboy_gui_cycle_palette(void);
const char* gameboy_gui_get_palette_name(void);
void gameboy_gui_reset(void);
void gameboy_gui_cleanup(void);

#endif // EMULATOR_GAMEBOY_H
