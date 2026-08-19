#ifndef LIBC_STRING_H
#define LIBC_STRING_H

#include <libc/stdint.h>

typedef unsigned int size_t;

int    strcmp(const char* s1, const char* s2);
int    strncmp(const char* s1, const char* s2, size_t n);
int    strcasecmp(const char* s1, const char* s2);
int    strncasecmp(const char* s1, const char* s2, size_t n);

char*  strcpy(char* dest, const char* src);
char*  strncpy(char* dest, const char* src, size_t n);
char*  strcat(char* dest, const char* src);
char*  strncat(char* dest, const char* src, size_t n);

size_t strlen(const char* str);
char*  strchr(const char* s, int c);
char*  strrchr(const char* s, int c);
char*  strstr(const char* haystack, const char* needle);
char*  strdup(const char* s);

void*  memset(void* dest, int val, size_t len);
void*  memcpy(void* dest, const void* src, size_t len);
void*  memmove(void* dest, const void* src, size_t len);
int    memcmp(const void* s1, const void* s2, size_t n);
char*  strupr(char* s);

#endif // LIBC_STRING_H
