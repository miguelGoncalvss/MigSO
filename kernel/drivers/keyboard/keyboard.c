#include <drivers/keyboard.h>
#include <drivers/vga.h>
#include <arch/i386/io.h>
#include <arch/i386/pic.h>
#include <arch/i386/idt.h>
#include <shell/shell.h>

// Constantes de Teclado compativeis com DOOM
#define DOOM_KEY_RIGHTARROW  0xae
#define DOOM_KEY_LEFTARROW   0xac
#define DOOM_KEY_UPARROW     0xad
#define DOOM_KEY_DOWNARROW   0xaf
#define DOOM_KEY_ESCAPE      27
#define DOOM_KEY_ENTER       13
#define DOOM_KEY_TAB         9
#define DOOM_KEY_F1          (0x80+0x3b)
#define DOOM_KEY_F2          (0x80+0x3c)
#define DOOM_KEY_F3          (0x80+0x3d)
#define DOOM_KEY_F4          (0x80+0x3e)
#define DOOM_KEY_F5          (0x80+0x3f)
#define DOOM_KEY_F6          (0x80+0x40)
#define DOOM_KEY_F7          (0x80+0x41)
#define DOOM_KEY_F8          (0x80+0x42)
#define DOOM_KEY_F9          (0x80+0x43)
#define DOOM_KEY_F10         (0x80+0x44)
#define DOOM_KEY_F11         (0x80+0x57)
#define DOOM_KEY_F12         (0x80+0x58)
#define DOOM_KEY_BACKSPACE   127
#define DOOM_KEY_PAUSE       0xff
#define DOOM_KEY_EQUALS      0x3d
#define DOOM_KEY_MINUS       0x2d
#define DOOM_KEY_RSHIFT      (0x80+0x36)
#define DOOM_KEY_RCTRL       (0x80+0x1d)
#define DOOM_KEY_RALT        (0x80+0x38)
#define DOOM_KEY_LALT        DOOM_KEY_RALT

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

typedef struct {
    int pressed;
    unsigned char doom_key;
} doom_key_event_t;

#define DOOM_QUEUE_SIZE 128
static doom_key_event_t doom_key_queue[DOOM_QUEUE_SIZE];
static volatile int doom_q_head = 0;
static volatile int doom_q_tail = 0;
static volatile int doom_mode_active = 0;

static char input_buffer[BUFFER_SIZE];
static int buffer_index = 0;
static int is_extended = 0;
static volatile int key_pressed_event = 0;

void keyboard_set_doom_mode(int enabled) {
    doom_mode_active = enabled;
    doom_q_head = 0;
    doom_q_tail = 0;
}

static void doom_enqueue_key(int pressed, unsigned char key) {
    if (!key) return;
    int next = (doom_q_head + 1) % DOOM_QUEUE_SIZE;
    if (next != doom_q_tail) {
        doom_key_queue[doom_q_head].pressed = pressed;
        doom_key_queue[doom_q_head].doom_key = key;
        doom_q_head = next;
    }
}

int keyboard_get_doom_key(int* pressed, unsigned char* doom_key) {
    if (doom_q_head == doom_q_tail) {
        return 0;
    }
    if (pressed)  *pressed  = doom_key_queue[doom_q_tail].pressed;
    if (doom_key) *doom_key = doom_key_queue[doom_q_tail].doom_key;
    doom_q_tail = (doom_q_tail + 1) % DOOM_QUEUE_SIZE;
    return 1;
}

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

static unsigned char translate_scancode_to_doom(unsigned char code, int extended) {
    if (extended) {
        switch (code) {
            case 0x48: return DOOM_KEY_UPARROW;
            case 0x50: return DOOM_KEY_DOWNARROW;
            case 0x4B: return DOOM_KEY_LEFTARROW;
            case 0x4D: return DOOM_KEY_RIGHTARROW;
            case 0x1D: return DOOM_KEY_RCTRL;
            case 0x38: return DOOM_KEY_RALT;
            default:   return 0;
        }
    }

    switch (code) {
        case 0x01: return DOOM_KEY_ESCAPE;
        case 0x0E: return DOOM_KEY_BACKSPACE;
        case 0x0F: return DOOM_KEY_TAB;
        case 0x1C: return DOOM_KEY_ENTER;
        case 0x1D: return DOOM_KEY_RCTRL;
        case 0x2A: return DOOM_KEY_RSHIFT;
        case 0x36: return DOOM_KEY_RSHIFT;
        case 0x38: return DOOM_KEY_RALT;
        case 0x39: return ' ';
        case 0x0C: return DOOM_KEY_MINUS;
        case 0x0D: return DOOM_KEY_EQUALS;
        case 0x3B: return DOOM_KEY_F1;
        case 0x3C: return DOOM_KEY_F2;
        case 0x3D: return DOOM_KEY_F3;
        case 0x3E: return DOOM_KEY_F4;
        case 0x3F: return DOOM_KEY_F5;
        case 0x40: return DOOM_KEY_F6;
        case 0x41: return DOOM_KEY_F7;
        case 0x42: return DOOM_KEY_F8;
        case 0x43: return DOOM_KEY_F9;
        case 0x44: return DOOM_KEY_F10;
        case 0x57: return DOOM_KEY_F11;
        case 0x58: return DOOM_KEY_F12;
        default:
            if (code < sizeof(scancode_ascii)) {
                return (unsigned char)scancode_ascii[code];
            }
            return 0;
    }
}

void keyboard_handler_c(void) {
    unsigned char raw_scancode = inb(0x60);
    int is_release = (raw_scancode & 0x80) ? 1 : 0;
    unsigned char code = raw_scancode & 0x7F;

    if (!is_release) {
        key_pressed_event = 1;
    }

    if (raw_scancode == 0xE0) {
        is_extended = 1;
        pic_send_eoi(1);
        return;
    }

    // Se o DOOM estiver em execucao, repassa todos os eventos de press e release
    if (doom_mode_active) {
        unsigned char dkey = translate_scancode_to_doom(code, is_extended);
        if (dkey) {
            doom_enqueue_key(is_release ? 0 : 1, dkey);
        }
        is_extended = 0;
        pic_send_eoi(1);
        return;
    }

    if (is_extended) {
        is_extended = 0;

        if (!is_release) {
            if (code == 0x48) { // Seta para CIMA
                if (!matrix_running) {
                    const char* prev_cmd = shell_history_up();
                    if (prev_cmd) replace_input_line(prev_cmd);
                }
            } else if (code == 0x50) { // Seta para BAIXO
                if (!matrix_running) {
                    const char* next_cmd = shell_history_down();
                    if (next_cmd) replace_input_line(next_cmd);
                }
            } else if (code == 0x49) { // Page Up
                vga_scroll_history_up(10);
            } else if (code == 0x51) { // Page Down
                vga_scroll_history_down(10);
            }
        }

        pic_send_eoi(1);
        return;
    }

    if (matrix_running) {
        pic_send_eoi(1);
        return;
    }

    if (!is_release) {
        if (code < (int)sizeof(scancode_ascii)) {
            char ch = scancode_ascii[code];

            if (ch == '\b') {
                if (buffer_index > 0) {
                    buffer_index--;
                    input_buffer[buffer_index] = '\0';
                    vga_putc('\b');
                }
            } else if (ch == '\n') {
                input_buffer[buffer_index] = '\0';
                vga_putc('\n');

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
    doom_mode_active = 0;
    doom_q_head = 0;
    doom_q_tail = 0;
    idt_set_gate(33, (unsigned int)keyboard_isr_wrapper);
}
