#ifndef _TIME_H
#define _TIME_H

#include <libc/stdint.h>

#ifndef _TIME_T_DEFINED
#define _TIME_T_DEFINED
typedef unsigned long time_t;
#endif

#ifndef _TM_DEFINED
#define _TM_DEFINED
struct tm {
    int tm_sec;    // Segundos [0-59]
    int tm_min;    // Minutos [0-59]
    int tm_hour;   // Horas [0-23]
    int tm_mday;   // Dia do mes [1-31]
    int tm_mon;    // Mes [0-11]
    int tm_year;   // Anos desde 1900
    int tm_wday;   // Dias desde domingo [0-6]
    int tm_yday;   // Dias desde 1 de janeiro [0-365]
    int tm_isdst;  // Horario de verao [-1/0/1]
};
#endif

time_t     time(time_t* tloc);
struct tm* localtime(const time_t* timer);
struct tm* gmtime(const time_t* timer);
time_t     mktime(struct tm* tm);
char*      asctime(const struct tm* tm);
char*      ctime(const time_t* timer);

#endif // _TIME_H
