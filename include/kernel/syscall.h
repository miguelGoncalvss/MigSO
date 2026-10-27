#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <libc/stdint.h>
#include <libc/stddef.h>
#include <arch/i386/isr.h>

// Numeros das chamadas de sistema (Syscall IDs)
#define SYS_EXIT        1
#define SYS_FORK        2
#define SYS_READ        3
#define SYS_WRITE       4
#define SYS_OPEN        5
#define SYS_CLOSE       6
#define SYS_WAITPID     7
#define SYS_EXECVE      11
#define SYS_TIME        13
#define SYS_GETPID      20
#define SYS_YIELD       24
#define SYS_SBRK        45
#define SYS_UPTIME      116
#define SYS_SLEEP       162
#define SYS_BEEP        200
#define SYS_PRINT_MEM   201

void syscall_init(void);
uint32_t syscall_handler_c(registers_t* regs);

// Funcao de validacao de seguranca de buffer em espaco de usuario
int syscall_validate_user_buffer(const void* ptr, size_t size, int write_check);

#endif // KERNEL_SYSCALL_H
