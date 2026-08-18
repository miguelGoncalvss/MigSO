#ifndef LIBC_STDLIB_H
#define LIBC_STDLIB_H

unsigned int rand(void);
void srand(unsigned int seed);
char* itoa(int value, char* str, int base);

#endif // LIBC_STDLIB_H
