#include <kernel/task.h>
#include <kernel/pmm.h>
#include <kernel/kheap.h>
#include <kernel/vmm.h>
#include <arch/i386/gdt.h>
#include <arch/i386/timer.h>
#include <drivers/vga.h>
#include <libc/string.h>
#include <libc/stdlib.h>

static task_t tasks[MAX_TASKS];
static task_t* current_task = NULL;
static int current_task_index = 0;
static int multitasking_active = 0;
static uint32_t next_pid = 1;

static void kthread_trampoline(void (*entry_point)(void)) {
    if (entry_point) {
        entry_point();
    }
    task_exit(0);
}

void task_init(void) {
    memset(tasks, 0, sizeof(tasks));

    // Inicializa a Tarefa 0 (Processo Principal do Kernel / Shell)
    tasks[0].pid = 0;
    strncpy(tasks[0].name, "kernel_main", 31);
    tasks[0].state = TASK_RUNNING;
    tasks[0].priority = 3;
    tasks[0].time_slice = DEFAULT_TIME_QUANTUM * 3;
    tasks[0].total_ticks = 0;
    tasks[0].sleep_ticks = 0;
    tasks[0].is_user = 0;
    tasks[0].exit_code = 0;
    tasks[0].page_directory = vmm_get_kernel_directory();

    // Configura a pilha inicial de kernel da tarefa 0
    tasks[0].kernel_stack_base = 0x00090000;
    tasks[0].esp0 = 0x0009FFFF;
    tasks[0].esp = 0;

    current_task = &tasks[0];
    current_task_index = 0;
    multitasking_active = 1;

    tss_set_kernel_stack(tasks[0].esp0);
}

task_t* task_get_current(void) {
    return current_task;
}

int task_is_multitasking_active(void) {
    return multitasking_active;
}

task_t* task_create_kthread(const char* name, void (*entry_point)(void), uint32_t priority) {
    if (!entry_point) return NULL;

    int slot = -1;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) {
            slot = i;
            break;
        }
    }

    if (slot == -1) return NULL; // Limite de tarefas atingido

    task_t* t = &tasks[slot];
    memset(t, 0, sizeof(task_t));

    t->pid = next_pid++;
    strncpy(t->name, name ? name : "kthread", 31);
    t->priority = (priority > 0 && priority <= 5) ? priority : 3;
    t->time_slice = t->priority * DEFAULT_TIME_QUANTUM;
    t->is_user = 0;
    t->page_directory = vmm_get_kernel_directory();

    // Aloca 1 frame (4 KB) para a pilha de execucao do Kernel Thread
    void* kstack_frame = pmm_alloc_block();
    if (!kstack_frame) {
        kstack_frame = kmalloc(KERNEL_STACK_SIZE);
    }
    if (!kstack_frame) {
        t->state = TASK_UNUSED;
        return NULL;
    }

    t->kernel_stack_base = (uint32_t)kstack_frame;
    t->esp0 = t->kernel_stack_base + KERNEL_STACK_SIZE;

    // Configura os argumentos e endereco de retorno falso para o trampoline
    uint32_t* stack_ptr = (uint32_t*)t->esp0;
    *(--stack_ptr) = (uint32_t)entry_point; // Argumento passado para kthread_trampoline
    *(--stack_ptr) = 0;                     // Endereco de retorno ficticio

    // Monta o frame inicial de registradores para o iret da troca de contexto
    registers_t* regs = (registers_t*)((uint32_t)stack_ptr - sizeof(registers_t));
    memset(regs, 0, sizeof(registers_t));

    regs->ds       = 0x10; // Kernel Data Segment
    regs->cs       = 0x08; // Kernel Code Segment
    regs->eip      = (uint32_t)kthread_trampoline;
    regs->eflags   = 0x0202; // IF = 1 (Interrupcoes habilitadas)
    regs->int_no   = 32;
    regs->err_code = 0;

    t->esp = (uint32_t)regs;
    t->state = TASK_READY;

    return t;
}

task_t* task_create_user_process(const char* name, void (*entry_point)(void), uint32_t priority) {
    if (!entry_point) return NULL;

    int slot = -1;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) {
            slot = i;
            break;
        }
    }

    if (slot == -1) return NULL;

    task_t* t = &tasks[slot];
    memset(t, 0, sizeof(task_t));

    t->pid = next_pid++;
    strncpy(t->name, name ? name : "user_proc", 31);
    t->priority = (priority > 0 && priority <= 5) ? priority : 3;
    t->time_slice = t->priority * DEFAULT_TIME_QUANTUM;
    t->is_user = 1;

    // 1. Cria um Page Directory proprio e isolado para o processo Ring 3
    page_directory_t* pdir = vmm_create_page_directory();
    if (!pdir) {
        t->state = TASK_UNUSED;
        return NULL;
    }
    t->page_directory = pdir;

    // 2. Aloca pilha de Kernel (Ring 0) usada pelo TSS nas interrupcoes/syscalls
    void* kstack_frame = pmm_alloc_block();
    if (!kstack_frame) {
        kstack_frame = kmalloc(KERNEL_STACK_SIZE);
    }
    if (!kstack_frame) {
        vmm_destroy_page_directory(pdir);
        t->state = TASK_UNUSED;
        return NULL;
    }
    t->kernel_stack_base = (uint32_t)kstack_frame;
    t->esp0 = t->kernel_stack_base + KERNEL_STACK_SIZE;

    // 3. Aloca e mapeia a Pilha de Usuario (Ring 3) em 0xBFFF0000 (Top: 0xBFFFF0)
    void* ustack_frame = pmm_alloc_block();
    if (!ustack_frame) {
        pmm_free_block(kstack_frame);
        vmm_destroy_page_directory(pdir);
        t->state = TASK_UNUSED;
        return NULL;
    }
    uint32_t user_stack_page = 0xBFFF0000;
    vmm_map_page(pdir, user_stack_page, (uint32_t)ustack_frame, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER);
    t->user_stack_top = 0xBFFFF0;

    // 4. Mapeia a pagina de codigo do ponto de entrada em espaco de usuario
    uint32_t entry_page = ((uint32_t)entry_point) & VMM_PAGE_FRAME_MASK;
    uint32_t phys_code = vmm_get_phys_addr(vmm_get_kernel_directory(), entry_page);
    if (!phys_code) phys_code = entry_page;
    vmm_map_page(pdir, entry_page, phys_code, VMM_PAGE_PRESENT | VMM_PAGE_USER);

    // Mapeia tambem a pagina seguinte de codigo se a funcao cruzar a borda de 4KB
    vmm_map_page(pdir, entry_page + PAGE_SIZE, phys_code + PAGE_SIZE, VMM_PAGE_PRESENT | VMM_PAGE_USER);

    // 5. Configura o frame de registradores na Pilha de Kernel para iret para Ring 3
    registers_t* regs = (registers_t*)(t->esp0 - sizeof(registers_t));
    memset(regs, 0, sizeof(registers_t));

    regs->ds       = 0x23; // User Data Segment (0x20 | 3)
    regs->cs       = 0x1B; // User Code Segment (0x18 | 3)
    regs->eip      = (uint32_t)entry_point;
    regs->eflags   = 0x0202; // IF = 1
    regs->useresp  = t->user_stack_top;
    regs->ss       = 0x23; // User Data Segment (0x20 | 3)
    regs->int_no   = 32;
    regs->err_code = 0;

    t->esp = (uint32_t)regs;
    t->state = TASK_READY;

    return t;
}

void task_yield(void) {
    if (!multitasking_active || !current_task) return;
    current_task->time_slice = 0;
    __asm__ volatile ("int $32");
}

void task_sleep(uint32_t ms) {
    if (!multitasking_active || !current_task) return;
    if (ms == 0) {
        task_yield();
        return;
    }

    uint32_t ticks = (ms * 100) / 1000;
    if (ticks == 0) ticks = 1;

    current_task->sleep_ticks = ticks;
    current_task->state = TASK_SLEEPING;
    current_task->time_slice = 0;

    __asm__ volatile ("int $32");
}

void task_exit(int code) {
    if (!current_task) return;

    current_task->exit_code = code;
    current_task->state = TASK_ZOMBIE;
    current_task->time_slice = 0;

    // Se nao for a thread principal do kernel, libera o espaco de paginas de usuario
    if (current_task->pid != 0) {
        if (current_task->is_user && current_task->page_directory) {
            // Limpa o diretorio de paginas do processo
            vmm_destroy_page_directory(current_task->page_directory);
            current_task->page_directory = vmm_get_kernel_directory();
        }
    }

    if (multitasking_active) {
        __asm__ volatile ("int $32");
    }

    while (1) {
        __asm__ volatile ("hlt");
    }
}

int task_kill(uint32_t pid) {
    if (pid == 0) return -1; // Protege a thread principal do kernel

    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].pid == pid && tasks[i].state != TASK_UNUSED) {
            tasks[i].exit_code = -9;
            tasks[i].state = TASK_ZOMBIE;

            if (tasks[i].is_user && tasks[i].page_directory) {
                vmm_destroy_page_directory(tasks[i].page_directory);
                tasks[i].page_directory = vmm_get_kernel_directory();
            }

            if (&tasks[i] == current_task) {
                task_yield();
            } else {
                tasks[i].state = TASK_UNUSED;
            }
            return 0;
        }
    }
    return -1;
}

uint32_t scheduler_tick(registers_t* regs) {
    if (!multitasking_active || !current_task) {
        return (uint32_t)regs;
    }

    // 1. Acorda processos adormecidos (TASK_SLEEPING)
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_SLEEPING) {
            if (tasks[i].sleep_ticks > 0) {
                tasks[i].sleep_ticks--;
            }
            if (tasks[i].sleep_ticks == 0) {
                tasks[i].state = TASK_READY;
            }
        }
    }

    current_task->total_ticks++;

    // 2. Decrementa o quantum do processo corrente
    if (current_task->state == TASK_RUNNING) {
        if (current_task->time_slice > 0) {
            current_task->time_slice--;
        }
    }

    // 3. Preempcao / Troca de Contexto se quantum expirou ou processo bloqueou/dormiu/morreu
    if (current_task->time_slice == 0 || current_task->state != TASK_RUNNING) {
        // Salva o ESP corrente no PCB da tarefa
        current_task->esp = (uint32_t)regs;

        if (current_task->state == TASK_RUNNING) {
            current_task->state = TASK_READY;
        }
        current_task->time_slice = current_task->priority * DEFAULT_TIME_QUANTUM;

        // Limpeza de tarefas ZOMBIE que ja foram substituidas
        for (int i = 1; i < MAX_TASKS; i++) {
            if (tasks[i].state == TASK_ZOMBIE && &tasks[i] != current_task) {
                tasks[i].state = TASK_UNUSED;
            }
        }

        // 4. Selecao da proxima tarefa pronta (Algoritmo Round-Robin)
        int current_idx = current_task_index;
        int next_idx = -1;

        for (int i = 1; i <= MAX_TASKS; i++) {
            int candidate = (current_idx + i) % MAX_TASKS;
            if (tasks[candidate].state == TASK_READY) {
                next_idx = candidate;
                break;
            }
        }

        // Se nenhuma outra tarefa estiver pronta, mantem a atual (se ainda estiver valida)
        if (next_idx == -1) {
            if (current_task->state == TASK_READY) {
                next_idx = current_idx;
            } else {
                next_idx = 0; // Retorna sempre ao Kernel Main / Shell
                tasks[0].state = TASK_READY;
            }
        }

        current_task_index = next_idx;
        current_task = &tasks[next_idx];
        current_task->state = TASK_RUNNING;

        // 5. Atualiza o TSS com a pilha de kernel (Ring 0) da nova tarefa para trocas seguras
        tss_set_kernel_stack(current_task->esp0);

        // 6. Troca o Page Directory se a nova tarefa possuir espaco de enderecamento proprio (CR3)
        if (current_task->page_directory && current_task->page_directory != vmm_get_current_directory()) {
            vmm_switch_page_directory(current_task->page_directory);
        }

        // 7. Retorna o ESP da nova tarefa para restauracao atomica dos registradores e iret
        return current_task->esp;
    }

    return (uint32_t)regs;
}

void task_list_print(void) {
    char buf[32];
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("================ LISTA DE PROCESSOS (PREEMPTIVE SCHEDULER) ================\n");
    vga_puts(" PID   NOME             ESTADO       PRIO  TICKS CPU  RING   CR3 (DIR)    ESP0 (KERN)\n");
    vga_puts("-----------------------------------------------------------------------------\n");

    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) continue;

        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        // PID
        itoa((int)tasks[i].pid, buf, 10);
        vga_puts(" "); vga_puts(buf);
        int plen = (int)strlen(buf);
        for (int s = 0; s < 5 - plen; s++) vga_putc(' ');

        // Nome
        vga_puts(tasks[i].name);
        int nlen = (int)strlen(tasks[i].name);
        for (int s = 0; s < 17 - nlen; s++) vga_putc(' ');

        // Estado
        switch (tasks[i].state) {
            case TASK_RUNNING:
                vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
                vga_puts("RUNNING     ");
                break;
            case TASK_READY:
                vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
                vga_puts("READY       ");
                break;
            case TASK_SLEEPING:
                vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
                vga_puts("SLEEPING    ");
                break;
            case TASK_BLOCKED:
                vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
                vga_puts("BLOCKED     ");
                break;
            case TASK_ZOMBIE:
                vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
                vga_puts("ZOMBIE      ");
                break;
            default:
                vga_puts("UNKNOWN     ");
                break;
        }

        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

        // Prioridade
        itoa((int)tasks[i].priority, buf, 10);
        vga_puts(buf); vga_puts("     ");

        // Ticks CPU
        itoa((int)tasks[i].total_ticks, buf, 10);
        vga_puts(buf);
        int tlen = (int)strlen(buf);
        for (int s = 0; s < 11 - tlen; s++) vga_putc(' ');

        // Ring
        if (tasks[i].is_user) {
            vga_set_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
            vga_puts("Ring 3 ");
        } else {
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            vga_puts("Ring 0 ");
        }

        // CR3
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        itoa((int)tasks[i].page_directory, buf, 16);
        vga_puts("0x"); vga_puts(buf);
        int clen = (int)strlen(buf) + 2;
        for (int s = 0; s < 13 - clen; s++) vga_putc(' ');

        // ESP0
        itoa((int)tasks[i].esp0, buf, 16);
        vga_puts("0x"); vga_puts(buf);

        vga_putc('\n');
    }
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("=============================================================================\n");
}
