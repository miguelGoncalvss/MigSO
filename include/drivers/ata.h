#ifndef DRIVERS_ATA_H
#define DRIVERS_ATA_H

#include <libc/stdint.h>
#include <libc/string.h>

#define ATA_SECTOR_SIZE     512
#define ATA_PRIMARY_DATA    0x1F0
#define ATA_PRIMARY_ERR     0x1F1
#define ATA_PRIMARY_SEC_CNT 0x1F2
#define ATA_PRIMARY_LBA_LO  0x1F3
#define ATA_PRIMARY_LBA_MID 0x1F4
#define ATA_PRIMARY_LBA_HI  0x1F5
#define ATA_PRIMARY_DRIVE   0x1F6
#define ATA_PRIMARY_CMD     0x1F7
#define ATA_PRIMARY_STATUS  0x1F7

#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_IDENTIFY    0xEC

#define ATA_STATUS_BSY      0x80
#define ATA_STATUS_DRDY     0x40
#define ATA_STATUS_DRQ      0x08
#define ATA_STATUS_ERR      0x01

// Inicializa e detecta o disco ATA / IDE primario
int ata_init(void);

// Le setores contiguos a partir do LBA especificado para a memoria
int ata_read_sectors(uint32_t lba, uint32_t count, void* buffer);

// Escreve setores contiguos a partir do LBA especificado
int ata_write_sectors(uint32_t lba, uint32_t count, const void* buffer);

// Retorna se o drive de disco ATA esta disponivel
int ata_is_available(void);

#endif // DRIVERS_ATA_H
