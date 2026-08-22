#include <libc/time.h>
#include <drivers/rtc.h>
#include <libc/stdio.h>
#include <libc/string.h>

static struct tm shared_tm;
static char shared_time_str[32];

time_t time(time_t* tloc) {
    time_t now = (time_t)rtc_get_unix_timestamp();
    if (tloc) {
        *tloc = now;
    }
    return now;
}

struct tm* gmtime(const time_t* timer) {
    if (!timer) return NULL;
    
    rtc_time_t rt;
    rtc_epoch_to_time((uint32_t)*timer, &rt);

    shared_tm.tm_sec  = rt.second;
    shared_tm.tm_min  = rt.minute;
    shared_tm.tm_hour = rt.hour;
    shared_tm.tm_mday = rt.day;
    shared_tm.tm_mon  = (rt.month > 0) ? rt.month - 1 : 0;
    shared_tm.tm_year = (rt.year >= 1900) ? rt.year - 1900 : 0;
    shared_tm.tm_wday = (rt.day_of_week > 0) ? rt.day_of_week - 1 : 0;
    shared_tm.tm_yday = 0;
    shared_tm.tm_isdst = 0;

    return &shared_tm;
}

struct tm* localtime(const time_t* timer) {
    return gmtime(timer);
}

time_t mktime(struct tm* tm) {
    if (!tm) return 0;
    rtc_time_t rt;
    rt.second = tm->tm_sec;
    rt.minute = tm->tm_min;
    rt.hour   = tm->tm_hour;
    rt.day    = tm->tm_mday;
    rt.month  = tm->tm_mon + 1;
    rt.year   = tm->tm_year + 1900;
    rt.day_of_week = tm->tm_wday + 1;

    return (time_t)rtc_time_to_epoch(&rt);
}

char* asctime(const struct tm* tm) {
    if (!tm) return NULL;
    static const char* mon_names[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    snprintf(shared_time_str, sizeof(shared_time_str), "%s %02d %02d:%02d:%02d %d\n",
             (tm->tm_mon >= 0 && tm->tm_mon < 12) ? mon_names[tm->tm_mon] : "???",
             tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_year + 1900);
    return shared_time_str;
}

char* ctime(const time_t* timer) {
    return asctime(localtime(timer));
}
