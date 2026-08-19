#include <drivers/keyboard.h>
#include <drivers/vga.h>
#include <arch/i386/io.h>
#include <arch/i386/pic.h>
#include <arch/i386/idt.h>
#include <shell/shell.h>

static const char scancode_ascii_lower[128] = {
    0,   27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, /* Ctrl */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, /* Left Shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0, /* Right Shift */
    '*',
    0, /* Alt */
    ' ', /* Space */
    0, /* CapsLock */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* F1 - F10 */
};

static const char scancode_ascii_upper[128] = {
    0,   27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, /* Ctrl */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, /* Left Shift */
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    0, /* Right Shift */
    '*',
    0, /* Alt */
    ' ', /* Space */
    0, /* CapsLock */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* F1 - F10 */
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

static volatile int shift_down = 0;
static volatile int ctrl_down = 0;
static volatile int alt_down = 0;
static volatile int caps_lock = 0;

static char input_buffer[BUFFER_SIZE];
static int buffer_index = 0;
static int is_extended = 0;
static volatile int key_pressed_event = 0;

int keyboard_is_ctrl_down(void) {
    return ctrl_down;
}

int keyboard_is_shift_down(void) {
    return shift_down;
}

int keyboard_is_alt_down(void) {
    return alt_down;
}

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

int keyboard_get_event(key_event_t* event) {
    if (!event) return 0;
    int pressed;
    unsigned char key;
    if (keyboard_get_doom_key(&pressed, &key)) {
        event->pressed = pressed;
        event->key = key;
        event->ctrl = ctrl_down;
        event->shift = shift_down;
        event->alt = alt_down;
        return 1;
    }
    return 0;
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
            case 0x48: return KEY_UP_ARROW;
            case 0x50: return KEY_DOWN_ARROW;
            case 0x4B: return KEY_LEFT_ARROW;
            case 0x4D: return KEY_RIGHT_ARROW;
            case 0x47: return KEY_HOME;
            case 0x4F: return KEY_END;
            case 0x49: return KEY_PAGEUP;
            case 0x51: return KEY_PAGEDOWN;
            case 0x52: return KEY_INSERT;
            case 0x53: return KEY_DELETE;
            case 0x1D: return KEY_RCTRL;
            case 0x38: return KEY_RALT;
            default:   return 0;
        }
    }

    switch (code) {
        case 0x01: return KEY_ESCAPE;
        case 0x0E: return KEY_BACKSPACE;
        case 0x0F: return KEY_TAB;
        case 0x1C: return KEY_ENTER;
        case 0x1D: return KEY_LCTRL;
        case 0x2A: return KEY_LSHIFT;
        case 0x36: return KEY_RSHIFT;
        case 0x38: return KEY_LALT;
        case 0x39: return ' ';
        case 0x3A: return 0; // CapsLock
        case 0x3B: return KEY_F1;
        case 0x3C: return KEY_F2;
        case 0x3D: return KEY_F3;
        case 0x3E: return KEY_F4;
        case 0x3F: return KEY_F5;
        case 0x40: return KEY_F6;
        case 0x41: return KEY_F7;
        case 0x42: return KEY_F8;
        case 0x43: return KEY_F9;
        case 0x44: return KEY_F10;
        case 0x57: return KEY_F11;
        case 0x58: return KEY_F12;
        default:
            if (code < sizeof(scancode_ascii_lower)) {
                int use_upper = shift_down;
                char lower_c = scancode_ascii_lower[code];
                if (caps_lock && lower_c >= 'a' && lower_c <= 'z') {
                    use_upper = !use_upper;
                }
                return use_upper ? (unsigned char)scancode_ascii_upper[code] : (unsigned char)scancode_ascii_lower[code];
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

    // Atualiza estado dos modificadores (Shift, Ctrl, Alt, CapsLock)
    if (!is_extended) {
        if (code == 0x2A || code == 0x36) { // Left / Right Shift
            shift_down = !is_release;
        } else if (code == 0x1D) { // Ctrl
            ctrl_down = !is_release;
        } else if (code == 0x38) { // Alt
            alt_down = !is_release;
        } else if (code == 0x3A && !is_release) { // CapsLock toggle
            caps_lock = !caps_lock;
        }
    } else {
        if (code == 0x1D) {
            ctrl_down = !is_release;
        } else if (code == 0x38) {
            alt_down = !is_release;
        }
    }

    // Se o modo direto (DOOM, Snake, Editor, GUI) estiver ativo:
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
        if (code < (int)sizeof(scancode_ascii_lower)) {
            int use_upper = shift_down;
            char lower_c = scancode_ascii_lower[code];
            if (caps_lock && lower_c >= 'a' && lower_c <= 'z') {
                use_upper = !use_upper;
            }
            char ch = use_upper ? scancode_ascii_upper[code] : scancode_ascii_lower[code];

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
    shift_down = 0;
    ctrl_down = 0;
    alt_down = 0;
    caps_lock = 0;
    idt_set_gate(33, (unsigned int)keyboard_isr_wrapper);
}
