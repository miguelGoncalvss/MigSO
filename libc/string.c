#include <libc/string.h>
#include <kernel/kheap.h>

static inline char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

int strcmp(const char* s1, const char* s2) {
    if (!s1 || !s2) return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    if (!s1 || !s2 || n == 0) return 0;
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strcasecmp(const char* s1, const char* s2) {
    if (!s1 || !s2) return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    while (*s1 && (to_lower(*s1) == to_lower(*s2))) {
        s1++;
        s2++;
    }
    return (unsigned char)to_lower(*s1) - (unsigned char)to_lower(*s2);
}

int strncasecmp(const char* s1, const char* s2, size_t n) {
    if (!s1 || !s2) return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    while (n > 0) {
        if (*s1 == '\0' || *s2 == '\0') {
            return (unsigned char)to_lower(*s1) - (unsigned char)to_lower(*s2);
        }
        if (to_lower(*s1) != to_lower(*s2)) {
            return (unsigned char)to_lower(*s1) - (unsigned char)to_lower(*s2);
        }
        s1++;
        s2++;
        n--;
    }
    return 0;
}

char* strcpy(char* dest, const char* src) {
    if (!dest || !src) return dest;
    char* ret = dest;
    while ((*dest++ = *src++));
    return ret;
}

char* strncpy(char* dest, const char* src, size_t n) {
    if (!dest || !src || n == 0) return dest;
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char* strcat(char* dest, const char* src) {
    if (!dest || !src) return dest;
    char* ret = dest;
    while (*dest) dest++;
    while ((*dest++ = *src++));
    return ret;
}

char* strncat(char* dest, const char* src, size_t n) {
    if (!dest || !src || n == 0) return dest;
    char* ret = dest;
    while (*dest) dest++;
    while (n && (*src)) {
        *dest++ = *src++;
        n--;
    }
    *dest = '\0';
    return ret;
}

size_t strlen(const char* str) {
    if (!str) return 0;
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

char* strchr(const char* s, int c) {
    if (!s) return NULL;
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return (c == 0) ? (char*)s : NULL;
}

char* strrchr(const char* s, int c) {
    if (!s) return NULL;
    const char* last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == 0) return (char*)s;
    return (char*)last;
}

char* strstr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    if (*needle == '\0') return (char*)haystack;

    size_t needle_len = strlen(needle);
    while (*haystack) {
        if (strncmp(haystack, needle, needle_len) == 0) {
            return (char*)haystack;
        }
        haystack++;
    }
    return NULL;
}

char* strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = (char*)kmalloc(len);
    if (copy) {
        memcpy(copy, s, len);
    }
    return copy;
}

void* memset(void* dest, int val, size_t len) {
    if (!dest || len == 0) return dest;
    unsigned char* ptr = (unsigned char*)dest;
    while (len--) {
        *ptr++ = (unsigned char)val;
    }
    return dest;
}

void* memcpy(void* dest, const void* src, size_t len) {
    if (!dest || !src || len == 0) return dest;
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (len--) {
        *d++ = *s++;
    }
    return dest;
}

void* memmove(void* dest, const void* src, size_t len) {
    if (!dest || !src || len == 0) return dest;
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;

    if (d < s) {
        while (len--) {
            *d++ = *s++;
        }
    } else {
        d += len;
        s += len;
        while (len--) {
            *--d = *--s;
        }
    }
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    if (!s1 || !s2 || n == 0) return 0;
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

int toupper(int c) {
    if (c >= 'a' && c <= 'z') return c - ('a' - 'A');
    return c;
}

int tolower(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

int isdigit(int c) {
    return (c >= '0' && c <= '9');
}

int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

int isalpha(int c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

int isprint(int c) {
    return (c >= 32 && c <= 126);
}

int _stricmp(const char* s1, const char* s2) {
    return strcasecmp(s1, s2);
}

int _strnicmp(const char* s1, const char* s2, size_t n) {
    return strncasecmp(s1, s2, n);
}

int stricmp(const char* s1, const char* s2) {
    return strcasecmp(s1, s2);
}

int strnicmp(const char* s1, const char* s2, size_t n) {
    return strncasecmp(s1, s2, n);
}

char* strupr(char* s) {
    if (!s) return NULL;
    char* p = s;
    while (*p) {
        if (*p >= 'a' && *p <= 'z') *p -= 32;
        p++;
    }
    return s;
}

char* _strupr(char* s) {
    return strupr(s);
}

// Aliases para chamadas geradas pelo GCC com __declspec(dllimport)
int (*_imp__toupper)(int) = toupper;
int (*_imp__tolower)(int) = tolower;
int (*_imp__isdigit)(int) = isdigit;
int (*_imp__isspace)(int) = isspace;
int (*_imp__isalpha)(int) = isalpha;
int (*_imp__isprint)(int) = isprint;

