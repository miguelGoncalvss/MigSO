#ifndef KERNEL_TASK_H
#define KERNEL_TASK_H

#include <libc/stdint.h>
#include <libc/stddef.h>
#include <arch/i386/isr.h>
#include <kernel/vmm.h>

#define MAX_TASKS               32
#define KERNEL_STACK_SIZE       4096
#define USER_STACK_SIZE         4096
#define DEFAULT_TIME_QUANTUM    2       // 2 ticks do PIT (20 ms a 100 Hz)

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_BLOCKED,
    TASK_ZOMBIE
} task_state_t;

typedef struct task {
    uint32_t pid;                       // Identificador unico do processo
    char name[32];                      // Nome do processo / thread
    task_state_t state;                 // Estado do ciclo de vida

    uint32_t esp;                       // ESP salvo durante troca de contexto
    uint32_t esp0;                      // Topo da pilha de Kernel carregado no TSS
    uint32_t user_stack_top;            // Topo da pilha de Usuario (se Ring 3)
    uint32_t kernel_stack_base;         // Base da pilha de Kernel alocada

    page_directory_t* page_directory;   // Diretorio de paginas da tarefa

    uint32_t sleep_ticks;               // Contador de ticks para acordar
    uint32_t priority;                  // Prioridade (1 = baixa, 5 = alta)
    uint32_t time_slice;                // Fatias de tempo restantes no quantum
    uint32_t total_ticks;               // Estatistica: tempo total de CPU

    int is_user;                        // 1 se processo Ring 3, 0 se Kernel Thread
    int exit_code;                      // Codigo de encerramento

    struct task* next;
} task_t;

void task_init(void);
task_t* task_get_current(void);
int  task_is_multitasking_active(void);

task_t* task_create_kthread(const char* name, void (*entry_point)(void), uint32_t priority);
task_t* task_create_user_process(const char* name, void (*entry_point)(void), uint32_t priority);

void task_yield(void);
void task_sleep(uint32_t ms);
void task_exit(int code);

uint32_t scheduler_tick(registers_t* regs);
void task_list_print(void);
int  task_kill(uint32_t pid);

#endif // KERNEL_TASK_H
