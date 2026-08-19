#include <drivers/mouse.h>
#include <drivers/vga.h>
#include <arch/i386/io.h>
#include <arch/i386/pic.h>
#include <arch/i386/idt.h>

static int has_wheel = 0;
static int mouse_cycle = 0;
static unsigned char mouse_packet[4];
static mouse_state_t cur_mouse_state;

extern void mouse_isr_wrapper(void);

static inline void mouse_wait_write(void) {
    int timeout = 100000;
    while ((inb(0x64) & 0x02) && --timeout) {
        io_wait();
    }
}

static inline void mouse_wait_read(void) {
    int timeout = 100000;
    while (!(inb(0x64) & 0x01) && --timeout) {
        io_wait();
    }
}

static void mouse_write(unsigned char val) {
    mouse_wait_write();
    outb(0x64, 0xD4); // Informa ao 8042 que o proximo byte eh para o mouse
    mouse_wait_write();
    outb(0x60, val);
}

static unsigned char mouse_read(void) {
    mouse_wait_read();
    return inb(0x60);
}

void mouse_init(void) {
    cur_mouse_state.x = VGA_WIDTH / 2;
    cur_mouse_state.y = VGA_HEIGHT / 2;
    cur_mouse_state.left_button = 0;
    cur_mouse_state.right_button = 0;
    cur_mouse_state.middle_button = 0;
    cur_mouse_state.scroll_delta = 0;
    mouse_cycle = 0;
    has_wheel = 0;

    // 1. Habilita o segundo canal PS/2 (porta auxiliar do mouse)
    mouse_wait_write();
    outb(0x64, 0xA8);

    // 2. Le o byte de configuracao do controlador 8042
    mouse_wait_write();
    outb(0x64, 0x20);
    mouse_wait_read();
    unsigned char status = inb(0x60);

    // Ativa interrupcoes do teclado (bit 0) e do mouse (bit 1)
    // Habilita os clocks de ambas as portas (limpa bits 4 e 5)
    status |= 0x03;
    status &= ~0x30;

    // Escreve de volta o byte de configuracao
    mouse_wait_write();
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, status);

    // 3. Reseta configuracoes para o padrao (Set Defaults)
    mouse_write(0xF6);
    mouse_read(); // ACK (0xFA)

    // 4. Habilita o envio continuo de pacotes de dados (Enable Data Reporting)
    mouse_write(0xF4);
    mouse_read(); // ACK (0xFA)

    // Limpa quaisquer bytes pendentes no buffer do controlador 8042
    while (inb(0x64) & 0x01) {
        inb(0x60);
    }

    // 5. Registra a ISR do Mouse no IDT (Vetor 44 / IRQ 12)
    idt_set_gate(44, (unsigned int)mouse_isr_wrapper);

    // 6. Desmascara IRQ2 (Cascata do Slave PIC) e IRQ12 (Mouse)
    pic_unmask_irq(2);
    pic_unmask_irq(12);
}

int mouse_has_wheel(void) {
    return has_wheel;
}

static int mouse_bound_min_x = 0;
static int mouse_bound_max_x = 79;
static int mouse_bound_min_y = 0;
static int mouse_bound_max_y = 24;

void mouse_set_bounds(int min_x, int min_y, int max_x, int max_y) {
    mouse_bound_min_x = min_x;
    mouse_bound_min_y = min_y;
    mouse_bound_max_x = max_x;
    mouse_bound_max_y = max_y;
    mouse_cycle = 0;
    if (cur_mouse_state.x < min_x) cur_mouse_state.x = min_x;
    if (cur_mouse_state.x > max_x) cur_mouse_state.x = max_x;
    if (cur_mouse_state.y < min_y) cur_mouse_state.y = min_y;
    if (cur_mouse_state.y > max_y) cur_mouse_state.y = max_y;
}

void mouse_set_position(int x, int y) {
    cur_mouse_state.x = x;
    cur_mouse_state.y = y;
    mouse_cycle = 0;
    if (cur_mouse_state.x < mouse_bound_min_x) cur_mouse_state.x = mouse_bound_min_x;
    if (cur_mouse_state.x > mouse_bound_max_x) cur_mouse_state.x = mouse_bound_max_x;
    if (cur_mouse_state.y < mouse_bound_min_y) cur_mouse_state.y = mouse_bound_min_y;
    if (cur_mouse_state.y > mouse_bound_max_y) cur_mouse_state.y = mouse_bound_max_y;
}

void mouse_get_state(mouse_state_t* state) {
    if (state) {
        *state = cur_mouse_state;
    }
}

mouse_state_t mouse_get_state_val(void) {
    return cur_mouse_state;
}

static void process_mouse_packet(void) {
    unsigned char flags = mouse_packet[0];

    // Se houver overflow em X ou Y, descarta o pacote
    if ((flags & 0x40) || (flags & 0x80)) {
        return;
    }

    int rel_x = (int)mouse_packet[1];
    int rel_y = (int)mouse_packet[2];

    // Extensao de sinal dos eixos X e Y
    if (flags & 0x10) {
        rel_x -= 256;
    }
    if (flags & 0x20) {
        rel_y -= 256;
    }

    // Atualiza estado dos botoes e coordenadas
    cur_mouse_state.left_button   = (flags & 0x01) ? 1 : 0;
    cur_mouse_state.right_button  = (flags & 0x02) ? 1 : 0;
    cur_mouse_state.middle_button = (flags & 0x04) ? 1 : 0;
    cur_mouse_state.x += rel_x;
    cur_mouse_state.y -= rel_y; // Inverte Y para corresponder a tela

    if (cur_mouse_state.x < mouse_bound_min_x) cur_mouse_state.x = mouse_bound_min_x;
    if (cur_mouse_state.x > mouse_bound_max_x) cur_mouse_state.x = mouse_bound_max_x;
    if (cur_mouse_state.y < mouse_bound_min_y) cur_mouse_state.y = mouse_bound_min_y;
    if (cur_mouse_state.y > mouse_bound_max_y) cur_mouse_state.y = mouse_bound_max_y;

    cur_mouse_state.scroll_delta = 0;
}

void mouse_handler_c(void) {
    unsigned char status = inb(0x64);

    while (status & 0x01) {
        unsigned char b = inb(0x60);

        if (status & 0x20) {
            // Byte pertencente ao Mouse PS/2 (porta auxiliar)
            if (mouse_cycle == 0) {
                // Byte 0: Bit 3 DEVE ser sempre 1 em qualquer pacote PS/2 valido
                if (b & 0x08) {
                    mouse_packet[0] = b;
                    mouse_cycle = 1;
                }
            } else if (mouse_cycle == 1) {
                mouse_packet[1] = b;
                mouse_cycle = 2;
            } else if (mouse_cycle == 2) {
                mouse_packet[2] = b;
                mouse_cycle = 0;
                process_mouse_packet();
            }
        }

        status = inb(0x64);
    }

    pic_send_eoi(12);
}
