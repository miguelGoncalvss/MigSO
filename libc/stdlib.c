#include <libc/stdlib.h>

static unsigned int rand_seed = 0x1337BEEF;

void srand(unsigned int seed) {
    rand_seed = seed ? seed : 0x1337BEEF;
}

unsigned int rand(void) {
    rand_seed ^= rand_seed << 13;
    rand_seed ^= rand_seed >> 17;
    rand_seed ^= rand_seed << 5;
    return rand_seed;
}

char* itoa(int value, char* str, int base) {
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }

    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    int tmp_value;

    if (value < 0 && base == 10) {
        *ptr++ = '-';
        str++;
        value = -value;
    }

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

    return str;
}
