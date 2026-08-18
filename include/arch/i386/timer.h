#ifndef ARCH_I386_TIMER_H
#define ARCH_I386_TIMER_H

#define PIT_BASE_FREQUENCY 1193182
#define PIT_COMMAND_PORT   0x43
#define PIT_CHANNEL0_PORT  0x40

void timer_init(unsigned int freq);
void timer_wait(unsigned int ticks);
void sleep(unsigned int ms);
unsigned int get_uptime(void);
unsigned int get_uptime_ms(void);
unsigned int timer_get_ticks(void);

#endif // ARCH_I386_TIMER_H
