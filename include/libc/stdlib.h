#ifndef LIBC_STDLIB_H
#define LIBC_STDLIB_H

#include <libc/stdint.h>
#include <libc/string.h>

// Gerenciamento de memoria dinamica (mapeadas para o Heap do Kernel migOS)
void* malloc(size_t size);
void  free(void* ptr);
void* calloc(size_t num, size_t size);
void* realloc(void* ptr, size_t new_size);

// Conversoes numericas e utilitarios
int          atoi(const char* str);
long         atol(const char* str);
long         strtol(const char* nptr, char** endptr, int base);
char*        itoa(int value, char* str, int base);
int          abs(int n);
unsigned int rand(void);
void         srand(unsigned int seed);

// Controle de fluxo e ambiente
void         exit(int status);
void         abort(void);
char*        getenv(const char* name);

// Algoritmo de ordenacao Quicksort
typedef int (*qsort_cmp)(const void*, const void*);
void         qsort(void* base, size_t num, size_t size, qsort_cmp compar);

#endif // LIBC_STDLIB_H
