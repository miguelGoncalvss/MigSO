#ifndef LIBC_SYSCALL_H
#define LIBC_SYSCALL_H

#include <libc/stdint.h>
#include <libc/stddef.h>

// Primitiva de chamada de sistema x86 via interrupcao int 0x80
static inline int syscall0(int num) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num) : "memory");
    return ret;
}

static inline int syscall1(int num, int arg1) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1) : "memory");
    return ret;
}

static inline int syscall2(int num, int arg1, int arg2) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2) : "memory");
    return ret;
}

static inline int syscall3(int num, int arg1, int arg2, int arg3) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3) : "memory");
    return ret;
}

// Wrappers amigaveis de chamadas de sistema para aplicacoes Ring 3
static inline void sys_exit(int code) {
    syscall1(1, code);
    while (1) {}
}

static inline int sys_read(int fd, void* buf, size_t count) {
    return syscall3(3, fd, (int)buf, (int)count);
}

static inline int sys_write(int fd, const void* buf, size_t count) {
    return syscall3(4, fd, (int)buf, (int)count);
}

static inline int sys_puts(const char* str) {
    size_t len = 0;
    if (!str) return 0;
    while (str[len]) len++;
    return syscall3(4, 1, (int)str, (int)len);
}

static inline int sys_getpid(void) {
    return syscall0(20);
}

static inline void sys_yield(void) {
    syscall0(24);
}

static inline void sys_sleep(uint32_t ms) {
    syscall1(162, (int)ms);
}

static inline uint32_t sys_uptime(void) {
    return (uint32_t)syscall0(116);
}

static inline void sys_beep(uint32_t freq, uint32_t ms) {
    syscall2(200, (int)freq, (int)ms);
}

#endif // LIBC_SYSCALL_H
