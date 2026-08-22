#ifndef DRIVERS_RTC_H
#define DRIVERS_RTC_H

#include <libc/stdint.h>
#include <libc/stdbool.h>
#include <libc/string.h>

// Portas de I/O da controladora CMOS / RTC
#define RTC_CMOS_ADDR_PORT   0x70
#define RTC_CMOS_DATA_PORT   0x71

// Registradores do CMOS / RTC
#define RTC_REG_SECONDS      0x00
#define RTC_REG_MINUTES      0x02
#define RTC_REG_HOURS        0x04
#define RTC_REG_WEEKDAY      0x06
#define RTC_REG_DAY          0x07
#define RTC_REG_MONTH        0x08
#define RTC_REG_YEAR         0x09
#define RTC_REG_STATUS_A     0x0A
#define RTC_REG_STATUS_B     0x0B
#define RTC_REG_STATUS_C     0x0C
#define RTC_REG_STATUS_D     0x0D
#define RTC_REG_CENTURY      0x32

// Bits de controle
#define RTC_STATUS_A_UIP     0x80 // Update In Progress
#define RTC_STATUS_B_24HR    0x02 // 24-hour format (1=24h, 0=12h)
#define RTC_STATUS_B_BINARY  0x04 // Binary mode (1=binario, 0=BCD)

// Estrutura representando data e hora do relogio de tempo real
typedef struct {
    uint8_t  second;       // 0-59
    uint8_t  minute;       // 0-59
    uint8_t  hour;         // 0-23
    uint8_t  day;          // 1-31
    uint8_t  month;        // 1-12
    uint16_t year;         // ex: 2026
    uint8_t  day_of_week;  // 1-7 (1=Domingo)
} rtc_time_t;

// Inicializa o driver de RTC
void     rtc_init(void);

// Le data e hora diretamente do hardware CMOS com decodificacao BCD e sincronizacao UIP
int      rtc_read_datetime(rtc_time_t* out_time);

// Obtem a data/hora atual (usando cache rapido ou leitura direta)
void     rtc_get_time(rtc_time_t* out_time);

// Obtem o timestamp UNIX atual (segundos desde 01/01/1970 00:00:00 UTC)
uint32_t rtc_get_unix_timestamp(void);

// Conversoes de tempo
uint32_t rtc_time_to_epoch(const rtc_time_t* t);
void     rtc_epoch_to_time(uint32_t epoch, rtc_time_t* out_time);

// Formatacao de strings
void     rtc_format_time(char* buf, size_t size);          // Formato "HH:MM"
void     rtc_format_time_full(char* buf, size_t size);     // Formato "HH:MM:SS"
void     rtc_format_date(char* buf, size_t size);          // Formato "DD/MM/YYYY"
void     rtc_format_datetime(char* buf, size_t size);      // Formato "DD/MM/YYYY HH:MM:SS"

// Formatacao de timestamp UNIX
void     rtc_format_epoch(uint32_t epoch, char* buf, size_t size);
void     rtc_format_epoch_short(uint32_t epoch, char* buf, size_t size); // "DD/MM HH:MM"

#endif // DRIVERS_RTC_H
