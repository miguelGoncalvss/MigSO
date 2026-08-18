#ifndef LIBC_STDINT_H
#define LIBC_STDINT_H

typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef long long          int64_t;

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef unsigned int       uintptr_t;
typedef int                intptr_t;

#ifndef NULL
#define NULL ((void*)0)
#endif

#endif // LIBC_STDINT_H
