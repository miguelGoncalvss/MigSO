#include <drivers/keyboard.h>
#include <drivers/vga.h>
#include <arch/i386/io.h>
#include <arch/i386/pic.h>
#include <arch/i386/idt.h>
#include <shell/shell.h>

static const char scancode_ascii[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, /* Ctrl */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, /* Left Shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0, /* Right Shift */
    '*',
    0, /* Alt */
    ' ', /* Space */
};

static char input_buffer[BUFFER_SIZE];
static int buffer_index = 0;
static int is_extended = 0;
static volatile int key_pressed_event = 0;

int keyboard_has_key(void) {
    return key_pressed_event;
}

void keyboard_clear_key(void) {
    key_pressed_event = 0;
}

static void replace_input_line(const char* new_text) {
    if (!new_text) return;

    while (buffer_index > 0) {
        vga_putc('\b');
        buffer_index--;
    }

    int i = 0;
    while (new_text[i] != '\0' && i < BUFFER_SIZE - 1) {
        input_buffer[i] = new_text[i];
        vga_putc(new_text[i]);
        i++;
    }
    buffer_index = i;
    input_buffer[buffer_index] = '\0';
}

void keyboard_handler_c(void) {
    unsigned char scancode = inb(0x60);

    // Sinaliza evento de tecla pressionada (bit 7 zerado = make code)
    if (!(scancode & 0x80)) {
        key_pressed_event = 1;
    }

    if (scancode == 0xE0) {
        is_extended = 1;
        pic_send_eoi(1);
        return;
    }

    if (is_extended) {
        is_extended = 0;

        if (!(scancode & 0x80)) {
            if (scancode == 0x48) { // Seta para CIMA
                if (!matrix_running) {
                    const char* prev_cmd = shell_history_up();
                    if (prev_cmd) {
                        replace_input_line(prev_cmd);
                    }
                }
            } else if (scancode == 0x50) { // Seta para BAIXO
                if (!matrix_running) {
                    const char* next_cmd = shell_history_down();
                    if (next_cmd) {
                        replace_input_line(next_cmd);
                    }
                }
            }
        }

        pic_send_eoi(1);
        return;
    }

    // Se o efeito matrix estiver em execucao, nao digita no terminal
    if (matrix_running) {
        pic_send_eoi(1);
        return;
    }

    if (!(scancode & 0x80)) {
        if (scancode < (int)sizeof(scancode_ascii)) {
            char ch = scancode_ascii[scancode];

            if (ch == '\b') {
                if (buffer_index > 0) {
                    buffer_index--;
                    input_buffer[buffer_index] = '\0';
                    vga_putc('\b');
                }
            } else if (ch == '\n') {
                input_buffer[buffer_index] = '\0';
                vga_putc('\n');

                // Envia comando para a fila do shell para execucao fora da ISR
                shell_post_command(input_buffer);
                buffer_index = 0;
                input_buffer[0] = '\0';
            } else if (ch) {
                if (buffer_index < BUFFER_SIZE - 1) {
                    input_buffer[buffer_index++] = ch;
                    input_buffer[buffer_index] = '\0';
                    vga_putc(ch);
                }
            }
        }
    }

    pic_send_eoi(1);
}

extern void keyboard_isr_wrapper(void);

void keyboard_init(void) {
    buffer_index = 0;
    input_buffer[0] = '\0';
    key_pressed_event = 0;
    idt_set_gate(33, (unsigned int)keyboard_isr_wrapper);
}

