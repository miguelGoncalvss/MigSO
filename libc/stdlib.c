#include <libc/stdlib.h>
#include <kernel/kheap.h>
#include <libc/string.h>
#include <arch/i386/reboot.h>

static unsigned int rand_seed = 0x1337BEEF;

void* malloc(size_t size) {
    return kmalloc(size);
}

void free(void* ptr) {
    kfree(ptr);
}

void* calloc(size_t num, size_t size) {
    return kcalloc(num, size);
}

void* realloc(void* ptr, size_t new_size) {
    return krealloc(ptr, new_size);
}

void srand(unsigned int seed) {
    rand_seed = seed ? seed : 0x1337BEEF;
}

unsigned int rand(void) {
    rand_seed ^= rand_seed << 13;
    rand_seed ^= rand_seed >> 17;
    rand_seed ^= rand_seed << 5;
    return rand_seed;
}

int abs(int n) {
    return (n < 0) ? -n : n;
}

int atoi(const char* str) {
    return (int)atol(str);
}

long atol(const char* str) {
    if (!str) return 0;
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }
    long sign = 1;
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    long val = 0;
    while (*str >= '0' && *str <= '9') {
        val = val * 10 + (*str - '0');
        str++;
    }
    return val * sign;
}

long strtol(const char* nptr, char** endptr, int base) {
    if (!nptr) return 0;
    const char* s = nptr;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') {
        s++;
    }
    long sign = 1;
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    } else if (base == 0) {
        base = (s[0] == '0') ? 8 : 10;
    }

    long val = 0;
    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9') digit = *s - '0';
        else if (*s >= 'a' && *s <= 'z') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') digit = *s - 'A' + 10;
        else break;

        if (digit >= base) break;
        val = val * base + digit;
        s++;
    }

    if (endptr) {
        *endptr = (char*)s;
    }
    return val * sign;
}

char* itoa(int value, char* str, int base) {
    if (!str) return NULL;
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }

    char* orig_str = str;
    char* ptr = str;
    char tmp_char;
    int tmp_value;

    if (value < 0 && base == 10) {
        *ptr++ = '-';
        // Handle INT_MIN safely
        if (value == -2147483648) {
            strcpy(orig_str, "-2147483648");
            return orig_str;
        }
        value = -value;
    }

    char* ptr1 = ptr;

    do {
        tmp_value = value;
        value /= base;
        int rem = tmp_value - value * base;
        if (rem < 0) rem = -rem;
        *ptr++ = "0123456789abcdef"[rem];
    } while (value);

    *ptr-- = '\0';

    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }

    return orig_str;
}

void exit(int status) {
    (void)status;
    // Em bare-metal, um exit fecha para o shell ou reinicia se critico
}

void abort(void) {
    __asm__ volatile ("int $0"); // Dispara kernel panic
}

char* getenv(const char* name) {
    (void)name;
    return NULL;
}

static void swap_bytes(char* a, char* b, size_t width) {
    while (width--) {
        char tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
}

void qsort(void* base, size_t num, size_t size, qsort_cmp compar) {
    if (num < 2 || size == 0 || !base || !compar) return;

    char* pivot = (char*)base + (num - 1) * size;
    size_t i = 0;

    for (size_t j = 0; j < num - 1; j++) {
        if (compar((char*)base + j * size, pivot) <= 0) {
            swap_bytes((char*)base + i * size, (char*)base + j * size, size);
            i++;
        }
    }
    swap_bytes((char*)base + i * size, pivot, size);

    if (i > 0) {
        qsort(base, i, size, compar);
    }
    if (i + 1 < num) {
        qsort((char*)base + (i + 1) * size, num - (i + 1), size, compar);
    }
}

double atof(const char* str) {
    if (!str) return 0.0;
    while (*str == ' ' || *str == '\t') str++;
    double sign = 1.0;
    if (*str == '-') { sign = -1.0; str++; }
    else if (*str == '+') { str++; }

    double val = 0.0;
    while (*str >= '0' && *str <= '9') {
        val = val * 10.0 + (*str - '0');
        str++;
    }
    if (*str == '.') {
        str++;
        double frac = 0.1;
        while (*str >= '0' && *str <= '9') {
            val += (*str - '0') * frac;
            frac *= 0.1;
            str++;
        }
    }
    return sign * val;
}

double fabs(double x) {
    return x < 0.0 ? -x : x;
}

float fabsf(float x) {
    return x < 0.0f ? -x : x;
}

// Rotinas auxiliares de 64-bit inteiros emitidas pelo GCC em 32-bit
long long __divdi3(long long a, long long b) {
    if (b == 0) return 0;
    int sign = 1;
    if (a < 0) { a = -a; sign = -sign; }
    if (b < 0) { b = -b; sign = -sign; }
    
    unsigned long long ua = (unsigned long long)a;
    unsigned long long ub = (unsigned long long)b;
    unsigned long long res = 0;
    
    for (int i = 63; i >= 0; i--) {
        if ((ub << i) <= ua && (ub << i) > 0) {
            ua -= (ub << i);
            res |= (1ULL << i);
        }
    }
    return sign < 0 ? -(long long)res : (long long)res;
}

unsigned long long __udivdi3(unsigned long long a, unsigned long long b) {
    if (b == 0) return 0;
    unsigned long long res = 0;
    for (int i = 63; i >= 0; i--) {
        if ((b << i) <= a && (b << i) > 0) {
            a -= (b << i);
            res |= (1ULL << i);
        }
    }
    return res;
}


