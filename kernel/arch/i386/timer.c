#include <arch/i386/timer.h>
#include <arch/i386/io.h>
#include <arch/i386/idt.h>
#include <arch/i386/pic.h>

static volatile unsigned int timer_ticks = 0;
static unsigned int timer_frequency = 100;

extern void timer_isr_wrapper(void);

void timer_handler_c(void) {
    timer_ticks++;
    pic_send_eoi(0);
}

void timer_init(unsigned int freq) {
    if (freq == 0) freq = 100;
    timer_frequency = freq;

    // Registra a ISR do Timer no Vetor 32 (IRQ 0) da IDT
    idt_set_gate(32, (unsigned int)timer_isr_wrapper);

    // Calcula o divisor da frequencia base do PIT (1.193182 MHz)
    unsigned int divisor = PIT_BASE_FREQUENCY / freq;
    if (divisor > 65535) divisor = 65535;
    if (divisor == 0) divisor = 1;

    // Porta 0x43: Byte de comando
    // 0x36 = 00110110b -> Canal 0, LSB/MSB, Modo 3 (Square Wave), Contador Binario
    outb(PIT_COMMAND_PORT, 0x36);
    io_wait();

    // Porta 0x40: Divisor (primeiro o byte baixo, depois o byte alto)
    outb(PIT_CHANNEL0_PORT, (unsigned char)(divisor & 0xFF));
    io_wait();
    outb(PIT_CHANNEL0_PORT, (unsigned char)((divisor >> 8) & 0xFF));
    io_wait();
}

void timer_wait(unsigned int ticks) {
    unsigned int target = timer_ticks + ticks;
    while (timer_ticks < target) {
        __asm__ volatile ("hlt"); // Pausa eficiente da CPU aguardando interrupcoes
    }
}

void sleep(unsigned int ms) {
    if (ms == 0) return;
    unsigned int ticks = (ms * timer_frequency) / 1000;
    if (ticks == 0) ticks = 1;
    timer_wait(ticks);
}

unsigned int get_uptime(void) {
    if (timer_frequency == 0) return 0;
    return timer_ticks / timer_frequency;
}

unsigned int get_uptime_ms(void) {
    if (timer_frequency == 0) return 0;
    return (timer_ticks * 1000) / timer_frequency;
}

unsigned int timer_get_ticks(void) {
    return timer_ticks;
}
