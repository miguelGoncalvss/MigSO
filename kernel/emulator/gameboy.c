#include <emulator/gameboy.h>
#define PEANUT_GB_12_COLOUR 0
#include <emulator/peanut_gb.h>
#include <gui/gui.h>
#include <drivers/bga.h>
#include <drivers/vga.h>
#include <drivers/keyboard.h>
#include <arch/i386/timer.h>
#include <kernel/kheap.h>
#include <fs/migfs.h>
#include <libc/stdio.h>
#include <libc/stdlib.h>
#include <libc/string.h>

// Contexto estatico do Peanut-GB e buffers
static struct gb_s gb_inst;
static uint8_t* gb_rom_ptr = NULL;
static size_t   gb_rom_sz = 0;
static uint8_t  gb_sram_buf[32768];
static char     gb_current_rom[MIGFS_MAX_FILENAME] = {0};
static char     gb_save_name[MIGFS_MAX_FILENAME] = {0};
static int      gb_sram_is_dirty = 0;
static uint32_t* gb_fb = NULL;

// Paletas de Cores do Game Boy (4 tonalidades)
// 0: DMG Classic Olive Green
static const uint32_t palette_dmg[4] = {
    0xFF9BBC0F, // 0: White / Light Olive
    0xFF8BAC0F, // 1: Light Green
    0xFF306230, // 2: Dark Green
    0xFF0F380F  // 3: Black / Dark Olive
};

// 1: Game Boy Pocket (Preto e Branco)
static const uint32_t palette_pocket[4] = {
    0xFFFFFFFF,
    0xFFAAAAAA,
    0xFF555555,
    0xFF000000
};

// 2: Super Game Boy Fire Red
static const uint32_t palette_fire_red[4] = {
    0xFFFFF8D0,
    0xFFF88850,
    0xFFC83838,
    0xFF481818
};

// 3: Cyber Cyan / Blue
static const uint32_t palette_cyan[4] = {
    0xFFE0FFFF,
    0xFF00D8FF,
    0xFF0058A8,
    0xFF001838
};

static int active_palette_idx = 0;

// Callbacks do Peanut-GB
static uint8_t gb_rom_read_cb(struct gb_s *gb, const uint_fast32_t addr) {
    (void)gb;
    if (addr < gb_rom_sz && gb_rom_ptr != NULL) {
        return gb_rom_ptr[addr];
    }
    return 0xFF;
}

static uint8_t gb_cart_ram_read_cb(struct gb_s *gb, const uint_fast32_t addr) {
    (void)gb;
    if (addr < sizeof(gb_sram_buf)) {
        return gb_sram_buf[addr];
    }
    return 0xFF;
}

static void gb_cart_ram_write_cb(struct gb_s *gb, const uint_fast32_t addr, const uint8_t val) {
    (void)gb;
    if (addr < sizeof(gb_sram_buf)) {
        gb_sram_buf[addr] = val;
        gb_sram_is_dirty = 1;
    }
}

static void gb_error_cb(struct gb_s *gb, const enum gb_error_e err, const uint_fast16_t val) {
    (void)gb;
    (void)err;
    (void)val;
}

// Callback de Renderizacao de Scanline (160x144 pixels)
// Renderizado no centro da tela 640x480 em escala 2x (320x288)
static void gb_lcd_draw_line_cb(struct gb_s *gb, const uint8_t *pixels, const uint_fast8_t line) {
    (void)gb;
    if (!gb_fb || line >= LCD_HEIGHT) return;

    const uint32_t* pal = palette_dmg;
    if (active_palette_idx == 1) pal = palette_pocket;
    else if (active_palette_idx == 2) pal = palette_fire_red;
    else if (active_palette_idx == 3) pal = palette_cyan;

    int base_x = (GUI_WIDTH - (LCD_WIDTH * 2)) / 2;  // 160
    int base_y = (GUI_HEIGHT - (LCD_HEIGHT * 2)) / 2; // 96

    int y1 = base_y + (line * 2);
    int y2 = y1 + 1;

    for (int x = 0; x < LCD_WIDTH; x++) {
        uint8_t color_idx = pixels[x] & 0x03;
        uint32_t px_color = pal[color_idx];

        int px1 = base_x + (x * 2);
        int px2 = px1 + 1;

        gb_fb[y1 * GUI_WIDTH + px1] = px_color;
        gb_fb[y1 * GUI_WIDTH + px2] = px_color;
        gb_fb[y2 * GUI_WIDTH + px1] = px_color;
        gb_fb[y2 * GUI_WIDTH + px2] = px_color;
    }
}

void gameboy_sync_save(void) {
    if (gb_sram_is_dirty && gb_save_name[0] != '\0') {
        migfs_write(gb_save_name, (const char*)gb_sram_buf, sizeof(gb_sram_buf));
        gb_sram_is_dirty = 0;
    }
}

static void draw_retro_bezel(const char* game_title) {
    // Fundo cinza escuro elegante estilo chassi de console
    for (int y = 0; y < GUI_HEIGHT; y++) {
        for (int x = 0; x < GUI_WIDTH; x++) {
            gb_fb[y * GUI_WIDTH + x] = 0xFF2B2B2B;
        }
    }

    // Barra superior com informacoes
    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < GUI_WIDTH; x++) {
            gb_fb[y * GUI_WIDTH + x] = 0xFF181818;
        }
    }

    // Borda da tela do Game Boy (Moldura 320x288 centralizada)
    int bx = (GUI_WIDTH - (LCD_WIDTH * 2)) / 2 - 8;
    int by = (GUI_HEIGHT - (LCD_HEIGHT * 2)) / 2 - 8;
    int bw = (LCD_WIDTH * 2) + 16;
    int bh = (LCD_HEIGHT * 2) + 16;

    for (int y = by; y < by + bh; y++) {
        for (int x = bx; x < bx + bw; x++) {
            gb_fb[y * GUI_WIDTH + x] = 0xFF101010;
        }
    }

    // Textos informativos
    gui_draw_string(12, 5, "migBoy Emulator (Peanut-GB v1.5)", 0xFF00D8FF);
    gui_draw_string(320, 5, game_title, 0xFFFFFF00);

    // Rodape com instrucoes de controle
    gui_draw_string(24, GUI_HEIGHT - 38, "[Setas / WASD] D-Pad   [Z/J] A   [X/K] B   [Enter] Start   [Espaco/C] Select", 0xFFFFFFFF);
    gui_draw_string(48, GUI_HEIGHT - 18, "[1-4] Trocar Paleta   [S] Salvar no Disco ATA   [ESC / Q] Sair", 0xFFAAAAAA);
}

void gameboy_print_cart_info(const char* rom_filename) {
    migfs_file_t* f = migfs_open(rom_filename);
    if (!f || !f->data || f->size < 0x150) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Erro: arquivo de ROM '");
        vga_puts(rom_filename);
        vga_puts("' nao encontrado ou invalido no disco MIGFS.\n");
        return;
    }

    char title[17] = {0};
    for (int i = 0; i < 16; i++) {
        char c = (char)f->data[0x134 + i];
        if (c >= ' ' && c <= '_') title[i] = c;
        else break;
    }

    char sz_buf[32];
    itoa((int)f->size, sz_buf, 10);

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("\nInformacoes do Cartucho Game Boy:\n");
    vga_puts("  Arquivo:     "); vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK); vga_puts(f->name); vga_putc('\n');
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("  Titulo:      "); vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK); vga_puts(title[0] ? title : "DESCONHECIDO"); vga_putc('\n');
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("  Tamanho:     "); vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK); vga_puts(sz_buf); vga_puts(" Bytes\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("  Tipo MBC:    "); vga_set_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    uint8_t cart_type = f->data[0x147];
    if (cart_type == 0x00) vga_puts("ROM Only (Sem MBC)\n");
    else if (cart_type >= 0x01 && cart_type <= 0x03) vga_puts("MBC1 (Pokemon Red/Blue/Yellow)\n");
    else if (cart_type >= 0x05 && cart_type <= 0x06) vga_puts("MBC2\n");
    else if (cart_type >= 0x0F && cart_type <= 0x13) vga_puts("MBC3 com RTC / SRAM (Pokemon Gold/Silver)\n");
    else if (cart_type >= 0x19 && cart_type <= 0x1E) vga_puts("MBC5\n");
    else vga_puts("Mapeador Especial\n");
}

int gameboy_launch(const char* rom_filename) {
    migfs_file_t* f = migfs_open(rom_filename);
    if (!f || !f->data || f->size < 0x150) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Erro: ROM '");
        vga_puts(rom_filename);
        vga_puts("' nao encontrada no disco persistente MIGFS!\n");
        vga_puts("Dica: grave o arquivo .gb no disco para iniciar o emulador.\n");
        return -1;
    }

    // Carrega ROM na memoria
    if (gb_rom_ptr) {
        kfree(gb_rom_ptr);
        gb_rom_ptr = NULL;
    }

    gb_rom_ptr = (uint8_t*)kmalloc(f->size);
    if (!gb_rom_ptr) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Erro: memoria insuficiente no Heap para alocar a ROM.\n");
        return -2;
    }

    memcpy(gb_rom_ptr, f->data, f->size);
    gb_rom_sz = f->size;
    strncpy(gb_current_rom, rom_filename, MIGFS_MAX_FILENAME - 1);

    // Prepara nome do save (ex: pokemon.gb -> pokemon.sav)
    strncpy(gb_save_name, rom_filename, MIGFS_MAX_FILENAME - 5);
    char* dot = strrchr(gb_save_name, '.');
    if (dot) strcpy(dot, ".sav");
    else strcat(gb_save_name, ".sav");

    // Limpa e restaura SRAM do save anterior se existir no disco ATA
    memset(gb_sram_buf, 0, sizeof(gb_sram_buf));
    gb_sram_is_dirty = 0;
    migfs_file_t* sav = migfs_open(gb_save_name);
    if (sav && sav->data && sav->size > 0) {
        size_t to_copy = (sav->size < sizeof(gb_sram_buf)) ? sav->size : sizeof(gb_sram_buf);
        memcpy(gb_sram_buf, sav->data, to_copy);
    }

    // Inicializa o nucleo do Peanut-GB
    enum gb_init_error_e err = gb_init(&gb_inst, gb_rom_read_cb, gb_cart_ram_read_cb, gb_cart_ram_write_cb, gb_error_cb, NULL);
    if (err != GB_INIT_NO_ERROR) {
        kfree(gb_rom_ptr);
        gb_rom_ptr = NULL;
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Erro ao inicializar Peanut-GB (codigo: ");
        char ebuf[16];
        itoa(err, ebuf, 10);
        vga_puts(ebuf);
        vga_puts(").\n");
        return -3;
    }

    gb_init_lcd(&gb_inst, gb_lcd_draw_line_cb);

    char game_title[32] = {0};
    gb_get_rom_name(&gb_inst, game_title);

    // Aloca backbuffer 640x480
    gb_fb = (uint32_t*)kmalloc(GUI_SCREEN_SIZE);
    if (!gb_fb) {
        kfree(gb_rom_ptr);
        gb_rom_ptr = NULL;
        return -4;
    }

    bga_init();
    keyboard_set_doom_mode(1);

    draw_retro_bezel(game_title[0] ? game_title : rom_filename);

    int running = 1;
    uint32_t last_sync_tick = timer_get_ticks();

    // Inicializa joypad com todos os botoes liberados (1 = solto)
    gb_inst.direct.joypad = 0xFF;

    while (running) {
        // Processa Teclado
        int pressed;
        unsigned char key;
        while (keyboard_get_doom_key(&pressed, &key)) {
            if (pressed) {
                if (key == KEY_ESCAPE || key == 'q' || key == 'Q') {
                    running = 0;
                } else if (key == KEY_UP_ARROW || key == 'w' || key == 'W') {
                    gb_inst.direct.joypad_bits.up = 0;
                } else if (key == KEY_DOWN_ARROW || key == 's' || key == 'S') {
                    gb_inst.direct.joypad_bits.down = 0;
                } else if (key == KEY_LEFT_ARROW || key == 'a' || key == 'A') {
                    gb_inst.direct.joypad_bits.left = 0;
                } else if (key == KEY_RIGHT_ARROW || key == 'd' || key == 'D') {
                    gb_inst.direct.joypad_bits.right = 0;
                } else if (key == 'z' || key == 'Z' || key == 'j' || key == 'J') {
                    gb_inst.direct.joypad_bits.a = 0;
                } else if (key == 'x' || key == 'X' || key == 'k' || key == 'K') {
                    gb_inst.direct.joypad_bits.b = 0;
                } else if (key == KEY_ENTER) {
                    gb_inst.direct.joypad_bits.start = 0;
                } else if (key == ' ' || key == KEY_BACKSPACE || key == 'c' || key == 'C') {
                    gb_inst.direct.joypad_bits.select = 0;
                } else if (key == '1') {
                    active_palette_idx = 0;
                } else if (key == '2') {
                    active_palette_idx = 1;
                } else if (key == '3') {
                    active_palette_idx = 2;
                } else if (key == '4') {
                    active_palette_idx = 3;
                } else if (key == 'p' || key == 'P') {
                    gameboy_sync_save();
                }
            } else {
                if (key == KEY_UP_ARROW || key == 'w' || key == 'W') {
                    gb_inst.direct.joypad_bits.up = 1;
                } else if (key == KEY_DOWN_ARROW || key == 's' || key == 'S') {
                    gb_inst.direct.joypad_bits.down = 1;
                } else if (key == KEY_LEFT_ARROW || key == 'a' || key == 'A') {
                    gb_inst.direct.joypad_bits.left = 1;
                } else if (key == KEY_RIGHT_ARROW || key == 'd' || key == 'D') {
                    gb_inst.direct.joypad_bits.right = 1;
                } else if (key == 'z' || key == 'Z' || key == 'j' || key == 'J') {
                    gb_inst.direct.joypad_bits.a = 1;
                } else if (key == 'x' || key == 'X' || key == 'k' || key == 'K') {
                    gb_inst.direct.joypad_bits.b = 1;
                } else if (key == KEY_ENTER) {
                    gb_inst.direct.joypad_bits.start = 1;
                } else if (key == ' ' || key == KEY_BACKSPACE || key == 'c' || key == 'C') {
                    gb_inst.direct.joypad_bits.select = 1;
                }
            }
        }

        // Executa 1 frame da CPU e PPU do Game Boy (70.224 ciclos)
        gb_run_frame(&gb_inst);

        // Renderiza frame no Linear Framebuffer BGA
        bga_blit(gb_fb);

        // Auto-sync do Save Game para o disco ATA a cada 5 segundos se modificado
        uint32_t now = timer_get_ticks();
        if (now - last_sync_tick >= 500) {
            gameboy_sync_save();
            last_sync_tick = now;
        }

        sleep(10);
    }

    // Salva progresso final no disco ATA antes de fechar
    gameboy_sync_save();

    keyboard_set_doom_mode(0);
    kfree(gb_fb);
    gb_fb = NULL;
    kfree(gb_rom_ptr);
    gb_rom_ptr = NULL;

    vga_clear();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("Emulador de Game Boy encerrado. Save persistido com sucesso!\n\n");

    return 0;
}
