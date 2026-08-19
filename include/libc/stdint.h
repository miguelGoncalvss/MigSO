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

typedef signed char        int_fast8_t;
typedef short              int_fast16_t;
typedef int                int_fast32_t;
typedef long long          int_fast64_t;

typedef unsigned char      uint_fast8_t;
typedef unsigned short     uint_fast16_t;
typedef unsigned int       uint_fast32_t;
typedef unsigned long long uint_fast64_t;

typedef signed char        int_least8_t;
typedef short              int_least16_t;
typedef int                int_least32_t;
typedef long long          int_least64_t;

typedef unsigned char      uint_least8_t;
typedef unsigned short     uint_least16_t;
typedef unsigned int       uint_least32_t;
typedef unsigned long long uint_least64_t;

typedef unsigned int       uintptr_t;
typedef int                intptr_t;
typedef long long          intmax_t;
typedef unsigned long long uintmax_t;

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef INT_MAX
#define INT_MAX    2147483647
#endif

#ifndef INT_MIN
#define INT_MIN    (-INT_MAX - 1)
#endif

#ifndef UINT_MAX
#define UINT_MAX   4294967295U
#endif

#ifndef LONG_MAX
#define LONG_MAX   2147483647L
#endif

#ifndef LONG_MIN
#define LONG_MIN   (-LONG_MAX - 1L)
#endif

#ifndef INT_FAST8_MAX
#define INT_FAST8_MAX    127
#endif
#ifndef INT_FAST16_MAX
#define INT_FAST16_MAX   32767
#endif
#ifndef INT_FAST32_MAX
#define INT_FAST32_MAX   2147483647
#endif
#ifndef UINT_FAST8_MAX
#define UINT_FAST8_MAX   255U
#endif
#ifndef UINT_FAST16_MAX
#define UINT_FAST16_MAX  65535U
#endif
#ifndef UINT_FAST32_MAX
#define UINT_FAST32_MAX  4294967295U
#endif

#endif // LIBC_STDINT_H

