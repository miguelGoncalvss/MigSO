#ifndef ARCH_I386_ISR_H
#define ARCH_I386_ISR_H

// Estrutura que representa o estado dos registradores na pilha apos a interrupcao
typedef struct {
    unsigned int ds;                                     // Segmento de dados
    unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax; // Empurrados pelo pusha
    unsigned int int_no, err_code;                       // Numero da interrupcao e codigo de erro
    unsigned int eip, cs, eflags, useresp, ss;           // Empurrados pela CPU automaticamente
} registers_t;

void isr_init(void);
void isr_handler_c(registers_t* regs);

#endif // ARCH_I386_ISR_H
