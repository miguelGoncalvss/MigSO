#ifndef ISR_H
#define ISR_H

// Estrutura que representa o estado dos registradores na pilha após a interrupção
typedef struct {
    unsigned int ds;                                     // Segmento de dados
    unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax; // Empurrados pelo pushal
    unsigned int int_no, err_code;                       // Número da interrupção e código de erro
    unsigned int eip, cs, eflags, useresp, ss;           // Empurrados pela CPU automaticamente
} registers_t;

void isr_init(void);

#endif