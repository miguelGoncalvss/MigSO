#ifndef LIBC_STRING_H
#define LIBC_STRING_H

typedef unsigned int size_t;

int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
size_t strlen(const char* str);
void* memset(void* dest, int val, size_t len);
void* memcpy(void* dest, const void* src, size_t len);

#endif // LIBC_STRING_H
