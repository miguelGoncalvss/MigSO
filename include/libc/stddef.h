#ifndef LIBC_STDDEF_H
#define LIBC_STDDEF_H

#include <libc/stdint.h>

#ifndef _SIZE_T_DEFINED
#define _SIZE_T_DEFINED
typedef unsigned int size_t;
#endif

#ifndef _PTRDIFF_T_DEFINED
#define _PTRDIFF_T_DEFINED
typedef int ptrdiff_t;
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef offsetof
#define offsetof(type, member) ((size_t)&(((type*)0)->member))
#endif

#endif // LIBC_STDDEF_H
