#include <kernel/syscall.h>
#include <kernel/task.h>
#include <kernel/vmm.h>
#include <kernel/kheap.h>
#include <arch/i386/idt.h>
#include <arch/i386/timer.h>
#include <drivers/vga.h>
#include <drivers/keyboard.h>
#include <drivers/sound.h>
#include <libc/string.h>
#include <libc/stdlib.h>

extern void syscall_isr_wrapper(void);

void syscall_init(void) {
    // Registra a ISR de Syscalls no Vetor 128 (0x80) com DPL 3 (Acessivel por Ring 3)
    idt_set_gate_user(128, (unsigned int)syscall_isr_wrapper);
}

int syscall_validate_user_buffer(const void* ptr, size_t size, int write_check) {
    if (!ptr || size == 0) return -1;

    task_t* cur = task_get_current();
    if (!cur || !cur->is_user) {
        return 0; // Kernel space threads possuem acesso total
    }

    uint32_t start = (uint32_t)ptr;
    uint32_t end   = start + size;

    if (end < start) return -1; // Overflow de ponteiro

    // Verifica se todas as paginas cobertas pelo buffer estao mapeadas
    page_directory_t* pdir = cur->page_directory ? cur->page_directory : vmm_get_current_directory();
    uint32_t page_start = start & VMM_PAGE_FRAME_MASK;
    uint32_t page_end   = (end + PAGE_SIZE - 1) & VMM_PAGE_FRAME_MASK;

    for (uint32_t p = page_start; p < page_end; p += PAGE_SIZE) {
        if (!vmm_is_mapped(pdir, p)) {
            return -1;
        }
    }

    return 0;
}

uint32_t syscall_handler_c(registers_t* regs) {
    uint32_t syscall_num = regs->eax;
    uint32_t arg1 = regs->ebx;
    uint32_t arg2 = regs->ecx;
    uint32_t arg3 = regs->edx;

    int32_t ret_val = 0;
    int needs_reschedule = 0;

    switch (syscall_num) {
        case SYS_EXIT: {
            task_exit((int)arg1);
            needs_reschedule = 1;
            break;
        }

        case SYS_GETPID: {
            task_t* cur = task_get_current();
            ret_val = cur ? (int32_t)cur->pid : 0;
            break;
        }

        case SYS_YIELD: {
            task_yield();
            needs_reschedule = 1;
            break;
        }

        case SYS_SLEEP: {
            task_sleep(arg1);
            needs_reschedule = 1;
            break;
        }

        case SYS_UPTIME: {
            ret_val = (int32_t)get_uptime();
            break;
        }

        case SYS_BEEP: {
            sound_beep(arg1, arg2);
            ret_val = 0;
            break;
        }

        case SYS_PRINT_MEM: {
            vmm_print_info();
            ret_val = 0;
            break;
        }

        case SYS_WRITE: {
            int fd = (int)arg1;
            const char* buf = (const char*)arg2;
            size_t count = (size_t)arg3;

            if (syscall_validate_user_buffer(buf, count, 0) != 0) {
                ret_val = -1; // EFAULT
                break;
            }

            if (fd == 1 || fd == 2) { // stdout ou stderr
                for (size_t i = 0; i < count; i++) {
                    vga_putc(buf[i]);
                }
                ret_val = (int32_t)count;
            } else {
                ret_val = -1;
            }
            break;
        }

        case SYS_READ: {
            int fd = (int)arg1;
            char* buf = (char*)arg2;
            size_t count = (size_t)arg3;

            if (syscall_validate_user_buffer(buf, count, 1) != 0) {
                ret_val = -1; // EFAULT
                break;
            }

            if (fd == 0 && count > 0) { // stdin
                size_t read_bytes = 0;
                while (read_bytes < count) {
                    key_event_t event;
                    if (keyboard_get_event(&event)) {
                        if (event.pressed && event.key != 0) {
                            buf[read_bytes++] = (char)event.key;
                            if (event.key == '\n' || event.key == '\r') break;
                        }
                    } else {
                        task_yield();
                    }
                }
                ret_val = (int32_t)read_bytes;
            } else {
                ret_val = -1;
            }
            break;
        }

        default: {
            ret_val = -1; // Syscall nao implementada
            break;
        }
    }

    regs->eax = (uint32_t)ret_val;

    if (needs_reschedule && task_is_multitasking_active()) {
        return scheduler_tick(regs);
    }

    return (uint32_t)regs;
}
