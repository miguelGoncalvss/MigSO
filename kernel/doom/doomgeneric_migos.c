#include <doom/doomgeneric.h>
#include <drivers/vga_mode13.h>
#include <drivers/vga.h>
#include <drivers/keyboard.h>
#include <drivers/ata.h>
#include <arch/i386/timer.h>
#include <kernel/kheap.h>
#include <fs/migfs.h>
#include <libc/stdio.h>
#include <libc/stdlib.h>
#include <libc/string.h>

extern void doomgeneric_Create(int argc, char **argv);
extern void doomgeneric_Tick(void);

void DG_Init(void) {
    // 1. Inicializa o hardware de vídeo no Modo 13h (320x200 com 256 cores)
    vga_set_mode_13h();

    // 2. Ativa a captura de interrupções de press/release no teclado PS/2
    keyboard_set_doom_mode(1);
}

void DG_DrawFrame(void) {
    // Copia o buffer de 64.000 pixels para o framebuffer linear da placa em 0xA0000
    if (DG_ScreenBuffer) {
        vga_mode13_blit((const uint8_t*)DG_ScreenBuffer);
    }
}

void DG_SleepMs(uint32_t ms) {
    // Pausa a execucao com o PIT Timer de 100 Hz
    sleep(ms);
}

uint32_t DG_GetTicksMs(void) {
    // Retorna o tempo de atividade do sistema em milissegundos
    return get_uptime_ms();
}

int DG_GetKey(int* pressed, unsigned char* doomKey) {
    // Le o proximo evento de tecla da fila assincrona
    return keyboard_get_doom_key(pressed, doomKey);
}

void DG_SetPalette(const uint8_t* palette) {
    // Atualiza a paleta DAC da VGA (256 cores RGB)
    vga_set_palette_all(palette);
}

void DG_SetWindowTitle(const char* title) {
    (void)title;
}

void doom_main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_puts("\n========================================\n");
    vga_puts("     DOOM BARE-METAL LOADER (migOS)     \n");
    vga_puts("========================================\n");

    // 1. Carrega o DOOM1.WAD do disco ATA para a memória se necessário
    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_puts("[DOOM] Carregando DOOM1.WAD do disco ATA (4.2 MB)...\n");
    int wad_status = load_doom_wad_from_disk();

    if (!migfs_exists("doom1.wad") && !migfs_exists("DOOM1.WAD")) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("[ERRO] Arquivo DOOM1.WAD nao encontrado! Codigo de erro: ");
        char buf[16];
        itoa(wad_status, buf, 10);
        vga_puts(buf);
        vga_puts("\nCertifique-se de que DOOM1.WAD esta presente na imagem migOS.img.\n\n");
        return;
    }

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("[OK] DOOM1.WAD localizado no RAMDisk.\n");
    vga_puts("[OK] Inicializando motor DOOM (Modo 13h 320x200 256 cores)...\n");
    sleep(600);

    char* doom_argv[] = { "doom", "-iwad", "doom1.wad", "-nosound", "-mb", "16" };
    int doom_argc = 6;

    doomgeneric_Create(doom_argc, doom_argv);

    while (1) {
        doomgeneric_Tick();
    }
}
