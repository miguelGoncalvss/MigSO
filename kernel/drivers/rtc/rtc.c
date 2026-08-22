#include <drivers/rtc.h>
#include <arch/i386/io.h>
#include <arch/i386/timer.h>
#include <libc/string.h>
#include <libc/stdio.h>

static rtc_time_t cached_time;
static uint32_t   cached_epoch = 0;
static uint32_t   last_update_tick = 0;
static int        rtc_initialized = 0;

static inline uint8_t rtc_read_reg(uint8_t reg) {
    outb(RTC_CMOS_ADDR_PORT, reg);
    return inb(RTC_CMOS_DATA_PORT);
}

static inline void rtc_write_reg(uint8_t reg, uint8_t val) {
    outb(RTC_CMOS_ADDR_PORT, reg);
    outb(RTC_CMOS_DATA_PORT, val);
}

static inline int rtc_is_updating(void) {
    outb(RTC_CMOS_ADDR_PORT, RTC_REG_STATUS_A);
    return (inb(RTC_CMOS_DATA_PORT) & RTC_STATUS_A_UIP);
}

static inline uint8_t bcd_to_binary(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static const int days_before_month[12] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

static const int days_in_month[12] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static inline int is_leap_year(uint16_t year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

int rtc_read_datetime(rtc_time_t* out_time) {
    if (!out_time) return -1;

    rtc_time_t t1, t2;
    uint8_t reg_b, century = 0;

    // Aguarda termino de atualizacao do relogio interno CMOS
    int timeout = 100000;
    while (rtc_is_updating() && --timeout > 0);

    // Leitura dupla para evitar inconsistencias durante a virada do segundo
    int retries = 5;
    do {
        timeout = 100000;
        while (rtc_is_updating() && --timeout > 0);

        t1.second  = rtc_read_reg(RTC_REG_SECONDS);
        t1.minute  = rtc_read_reg(RTC_REG_MINUTES);
        t1.hour    = rtc_read_reg(RTC_REG_HOURS);
        t1.day     = rtc_read_reg(RTC_REG_DAY);
        t1.month   = rtc_read_reg(RTC_REG_MONTH);
        t1.year    = rtc_read_reg(RTC_REG_YEAR);
        t1.day_of_week = rtc_read_reg(RTC_REG_WEEKDAY);

        timeout = 100000;
        while (rtc_is_updating() && --timeout > 0);

        t2.second  = rtc_read_reg(RTC_REG_SECONDS);
        t2.minute  = rtc_read_reg(RTC_REG_MINUTES);
        t2.hour    = rtc_read_reg(RTC_REG_HOURS);
        t2.day     = rtc_read_reg(RTC_REG_DAY);
        t2.month   = rtc_read_reg(RTC_REG_MONTH);
        t2.year    = rtc_read_reg(RTC_REG_YEAR);
        t2.day_of_week = rtc_read_reg(RTC_REG_WEEKDAY);

    } while ((t1.second != t2.second || t1.minute != t2.minute || t1.hour != t2.hour ||
              t1.day != t2.day || t1.month != t2.month || t1.year != t2.year) && --retries > 0);

    // Tenta ler o registrador de Seculo (0x32 padrao em PC/QEMU)
    century = rtc_read_reg(RTC_REG_CENTURY);

    reg_b = rtc_read_reg(RTC_REG_STATUS_B);

    // Decodifica formato BCD (Binary-Coded Decimal) caso necessario
    int is_bcd = !(reg_b & RTC_STATUS_B_BINARY);
    if (is_bcd) {
        t1.second = bcd_to_binary(t1.second);
        t1.minute = bcd_to_binary(t1.minute);
        t1.hour   = ((t1.hour & 0x7F) ? bcd_to_binary(t1.hour & 0x7F) : 0) | (t1.hour & 0x80);
        t1.day    = bcd_to_binary(t1.day);
        t1.month  = bcd_to_binary(t1.month);
        t1.year   = bcd_to_binary((uint8_t)t1.year);
        if (century != 0 && century != 0xFF) {
            century = bcd_to_binary(century);
        }
    }

    // Converte formato 12 horas (AM/PM) para 24 horas caso necessario
    if (!(reg_b & RTC_STATUS_B_24HR) && (t1.hour & 0x80)) {
        t1.hour = ((t1.hour & 0x7F) + 12) % 24;
    }

    // Calcula o ano completo (4 digitos)
    if (century >= 19 && century <= 22) {
        t1.year = (century * 100) + t1.year;
    } else {
        if (t1.year < 80) {
            t1.year += 2000;
        } else {
            t1.year += 1900;
        }
    }

    // Validacoes de sanidade
    if (t1.second > 59) t1.second = 0;
    if (t1.minute > 59) t1.minute = 0;
    if (t1.hour > 23)   t1.hour = 0;
    if (t1.month < 1 || t1.month > 12) t1.month = 1;
    if (t1.day < 1 || t1.day > 31)     t1.day = 1;

    *out_time = t1;
    return 0;
}

void rtc_init(void) {
    // Garante que o relogio nao esta gerando interrupcoes indesejadas
    uint8_t reg_b = rtc_read_reg(RTC_REG_STATUS_B);
    rtc_write_reg(RTC_REG_STATUS_B, reg_b | RTC_STATUS_B_24HR);

    rtc_read_datetime(&cached_time);
    cached_epoch = rtc_time_to_epoch(&cached_time);
    last_update_tick = timer_get_ticks();
    rtc_initialized = 1;
}

void rtc_get_time(rtc_time_t* out_time) {
    if (!out_time) return;

    uint32_t cur_tick = timer_get_ticks();
    // Atualiza a cada 5 ticks (~50ms) ou no primeiro acesso
    if (!rtc_initialized || (cur_tick - last_update_tick >= 5) || (cur_tick < last_update_tick)) {
        if (rtc_read_datetime(&cached_time) == 0) {
            cached_epoch = rtc_time_to_epoch(&cached_time);
            last_update_tick = cur_tick;
            rtc_initialized = 1;
        }
    }

    *out_time = cached_time;
}

uint32_t rtc_get_unix_timestamp(void) {
    rtc_time_t t;
    rtc_get_time(&t);
    return cached_epoch;
}

uint32_t rtc_time_to_epoch(const rtc_time_t* t) {
    if (!t || t->year < 1970 || t->month < 1 || t->month > 12 || t->day < 1 || t->day > 31) {
        return 0;
    }

    uint32_t days = 0;
    for (uint16_t y = 1970; y < t->year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }

    days += days_before_month[t->month - 1];
    if (t->month > 2 && is_leap_year(t->year)) {
        days += 1;
    }
    days += (t->day - 1);

    uint32_t seconds = (days * 86400) +
                       ((uint32_t)t->hour * 3600) +
                       ((uint32_t)t->minute * 60) +
                       (uint32_t)t->second;

    return seconds;
}

void rtc_epoch_to_time(uint32_t epoch, rtc_time_t* out_time) {
    if (!out_time) return;
    memset(out_time, 0, sizeof(rtc_time_t));

    if (epoch == 0) {
        out_time->year = 2026;
        out_time->month = 1;
        out_time->day = 1;
        return;
    }

    uint32_t sec = epoch % 86400;
    uint32_t days = epoch / 86400;

    out_time->second = sec % 60;
    out_time->minute = (sec / 60) % 60;
    out_time->hour   = sec / 3600;

    uint16_t y = 1970;
    while (1) {
        uint32_t d_in_y = is_leap_year(y) ? 366 : 365;
        if (days < d_in_y) break;
        days -= d_in_y;
        y++;
    }
    out_time->year = y;

    uint8_t m = 0;
    for (m = 0; m < 12; m++) {
        uint32_t dim = days_in_month[m];
        if (m == 1 && is_leap_year(y)) dim = 29;
        if (days < dim) break;
        days -= dim;
    }
    out_time->month = m + 1;
    out_time->day = (uint8_t)days + 1;
}

void rtc_format_time(char* buf, size_t size) {
    if (!buf || size < 6) return;
    rtc_time_t t;
    rtc_get_time(&t);
    snprintf(buf, size, "%02u:%02u", (unsigned int)t.hour, (unsigned int)t.minute);
}

void rtc_format_time_full(char* buf, size_t size) {
    if (!buf || size < 9) return;
    rtc_time_t t;
    rtc_get_time(&t);
    snprintf(buf, size, "%02u:%02u:%02u", (unsigned int)t.hour, (unsigned int)t.minute, (unsigned int)t.second);
}

void rtc_format_date(char* buf, size_t size) {
    if (!buf || size < 11) return;
    rtc_time_t t;
    rtc_get_time(&t);
    snprintf(buf, size, "%02u/%02u/%04u", (unsigned int)t.day, (unsigned int)t.month, (unsigned int)t.year);
}

void rtc_format_datetime(char* buf, size_t size) {
    if (!buf || size < 20) return;
    rtc_time_t t;
    rtc_get_time(&t);
    snprintf(buf, size, "%02u/%02u/%04u %02u:%02u:%02u",
             (unsigned int)t.day, (unsigned int)t.month, (unsigned int)t.year,
             (unsigned int)t.hour, (unsigned int)t.minute, (unsigned int)t.second);
}

void rtc_format_epoch(uint32_t epoch, char* buf, size_t size) {
    if (!buf || size < 20) return;
    rtc_time_t t;
    rtc_epoch_to_time(epoch, &t);
    snprintf(buf, size, "%02u/%02u/%04u %02u:%02u:%02u",
             (unsigned int)t.day, (unsigned int)t.month, (unsigned int)t.year,
             (unsigned int)t.hour, (unsigned int)t.minute, (unsigned int)t.second);
}

void rtc_format_epoch_short(uint32_t epoch, char* buf, size_t size) {
    if (!buf || size < 12) return;
    rtc_time_t t;
    rtc_epoch_to_time(epoch, &t);
    snprintf(buf, size, "%02u/%02u %02u:%02u",
             (unsigned int)t.day, (unsigned int)t.month,
             (unsigned int)t.hour, (unsigned int)t.minute);
}
