#include "shell.h"
#include "vga.h"
#include "io.h"

volatile int matrix_running = 0;

static char history[HISTORY_MAX][BUFFER_SIZE];
static int history_count = 0;
static int history_index = 0;

static unsigned int rand_seed = 0x1337BEEF;

static unsigned int rand(void) {
    rand_seed ^= rand_seed << 13;
    rand_seed ^= rand_seed >> 17;
    rand_seed ^= rand_seed << 5;
    return rand_seed;
}

// Comparador padrão de strings exato
static int kstrcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static void kstrcpy(char* dest, const char* src) {
    int i = 0;
    while (src[i] != '\0' && i < BUFFER_SIZE - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void shell_history_add(const char* command) {
    if (command[0] == '\0') return;

    if (history_count > 0 && kstrcmp(history[history_count - 1], command) == 0) {
        history_index = history_count;
        return;
    }

    if (history_count < HISTORY_MAX) {
        kstrcpy(history[history_count], command);
        history_count++;
    } else {
        for (int i = 0; i < HISTORY_MAX - 1; i++) {
            kstrcpy(history[i], history[i + 1]);
        }
        kstrcpy(history[HISTORY_MAX - 1], command);
    }
    history_index = history_count;
}

const char* shell_history_up(void) {
    if (history_count == 0) return 0;
    if (history_index > 0) {
        history_index--;
    }
    return history[history_index];
}

const char* shell_history_down(void) {
    if (history_count == 0) return 0;
    if (history_index < history_count - 1) {
        history_index++;
        return history[history_index];
    } else {
        history_index = history_count;
        return "";
    }
}

// Reinicialização forçada via Triple Fault (Garantido no QEMU e x86 Bare-Metal)
static void reboot_system(void) {
    vga_clear();
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("Reiniciando o migOS...\n");

    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);
    outb(0x64, 0xFE);

    // Carrega IDT com limite 0 e dispara interrupção (Força Reset de Hardware)
    __asm__ volatile (
        "cli\n"
        "pushl $0\n"
        "pushl $0\n"
        "lidt (%esp)\n"
        "int $3\n"
    );

    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

static int check_key_pressed(void) {
    if (inb(0x64) & 1) {
        inb(0x60);
        return 1;
    }
    return 0;
}

static int sleep_with_exit_check(unsigned int ms) {
    for (unsigned int i = 0; i < ms; i++) {
        if (check_key_pressed()) return 1;
        for (volatile int j = 0; j < 50000; j++) {
            __asm__ volatile ("nop");
        }
    }
    return 0;
}

static void matrix_effect(void) {
    vga_clear();

    int drops[VGA_WIDTH];
    for (int x = 0; x < VGA_WIDTH; x++) {
        drops[x] = -(int)(rand() % VGA_HEIGHT);
    }

    for (volatile int i = 0; i < 2000000; i++) {
        if (inb(0x64) & 1) inb(0x60);
    }

    while (1) {
        if (check_key_pressed()) break;

        for (int x = 0; x < VGA_WIDTH; x++) {
            if (rand() % 3 != 0) continue;

            int y = drops[x];

            if (y >= 0 && y < VGA_HEIGHT) {
                char ch = (rand() % 2 == 0) ? '0' : '1';
                VGA_MEMORY[y * VGA_WIDTH + x] = (unsigned short)ch | ((unsigned short)VGA_COLOR_WHITE << 8);
            }

            if ((y - 1) >= 0 && (y - 1) < VGA_HEIGHT) {
                char ch = (rand() % 2 == 0) ? '0' : '1';
                VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = (unsigned short)ch | ((unsigned short)VGA_COLOR_LIGHT_GREEN << 8);
            }

            if ((y - 2) >= 0 && (y - 2) < VGA_HEIGHT) {
                char ch = (rand() % 2 == 0) ? '0' : '1';
                VGA_MEMORY[(y - 2) * VGA_WIDTH + x] = (unsigned short)ch | ((unsigned short)VGA_COLOR_GREEN << 8);
            }

            if ((y - 8) >= 0 && (y - 8) < VGA_HEIGHT) {
                VGA_MEMORY[(y - 8) * VGA_WIDTH + x] = (unsigned short)' ' | ((unsigned short)VGA_COLOR_BLACK << 8);
            }

            drops[x]++;
            if (drops[x] - 8 >= VGA_HEIGHT) drops[x] = 0;
        }

        if (sleep_with_exit_check(100)) break;
    }

    vga_clear();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("Bem-vindo de volta ao mundo real, Neo.\n\n");
}

void shell_init(void) {
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("migOS> ");
}

void shell_execute(const char* command) {
    while (*command == ' ') command++;

    if (command[0] == '\0') {
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_puts("migOS> ");
        return;
    }

    shell_history_add(command);

    if (kstrcmp(command, "help") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        vga_puts("Comandos disponiveis:\n");
        vga_puts("  help     - Exibe esta lista de comandos\n");
        vga_puts("  clear    - Limpa a tela do terminal\n");
        vga_puts("  matrix   - Inicia a chuva de codigos Matrix\n");
        vga_puts("  version  - Exibe a versao atual do kernel\n");
        vga_puts("  about    - Informacoes sobre o autor e o sistema\n");
        vga_puts("  panic    - Dispara Kernel Panic de teste (Excecao ISR 0)\n");
        vga_puts("  reboot   - Reinicia a maquina virtual\n");
    } else if (kstrcmp(command, "clear") == 0) {
        vga_clear();
    } else if (kstrcmp(command, "matrix") == 0) {
        matrix_effect();
    } else if (kstrcmp(command, "version") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("migOS Kernel v0.5 (32-bit Protected Mode)\n");
    } else if (kstrcmp(command, "about") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
        vga_puts("migOS - Sistema Operacional Desenvolvido por Miguel\n");
        vga_puts("Arquitetura: x86 (IA-32) Bare-Metal\n");
    } else if (kstrcmp(command, "panic") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Disparando excecao de CPU (ISR 0)...\n");
        __asm__ volatile ("int $0");
    } else if (kstrcmp(command, "reboot") == 0) {
        reboot_system();
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Comando desconhecido: '");
        vga_puts(command);
        vga_puts("'\n");
    }

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("migOS> ");
}