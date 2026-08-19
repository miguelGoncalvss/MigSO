#ifndef LIBC_MATH_H
#define LIBC_MATH_H

#include <libc/stdint.h>

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

static inline int abs_val(int x) {
    return (x < 0) ? -x : x;
}

#endif // LIBC_MATH_H
