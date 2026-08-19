#include <drivers/ata.h>
#include <arch/i386/io.h>

static int ata_available = 0;

static int ata_wait_ready(void) {
    int timeout = 100000;
    while (timeout--) {
        unsigned char status = inb(ATA_PRIMARY_STATUS);
        if (!(status & ATA_STATUS_BSY)) {
            return 0;
        }
    }
    return -1;
}

static int ata_wait_drq(void) {
    int timeout = 100000;
    while (timeout--) {
        unsigned char status = inb(ATA_PRIMARY_STATUS);
        if (status & ATA_STATUS_ERR) {
            return -1;
        }
        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_DRQ)) {
            return 0;
        }
    }
    return -1;
}

int ata_init(void) {
    outb(ATA_PRIMARY_DRIVE, 0xA0);
    io_wait();

    unsigned char status = inb(ATA_PRIMARY_STATUS);
    if (status == 0xFF || status == 0x00) {
        ata_available = 0;
        return -1;
    }

    ata_available = 1;
    return 0;
}

int ata_is_available(void) {
    return ata_available;
}

int ata_read_sectors(uint32_t lba, uint32_t count, void* buffer) {
    if (!buffer || count == 0) return -1;

    uint16_t* ptr = (uint16_t*)buffer;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t cur_lba = lba + i;

        if (ata_wait_ready() != 0) return -2;

        outb(ATA_PRIMARY_DRIVE, (unsigned char)(0xE0 | ((cur_lba >> 24) & 0x0F)));
        // 400ns delay apos selecao de drive
        inb(ATA_PRIMARY_STATUS);
        inb(ATA_PRIMARY_STATUS);
        inb(ATA_PRIMARY_STATUS);
        inb(ATA_PRIMARY_STATUS);

        if (ata_wait_ready() != 0) return -2;

        outb(ATA_PRIMARY_ERR, 0x00);
        outb(ATA_PRIMARY_SEC_CNT, 1);
        outb(ATA_PRIMARY_LBA_LO, (unsigned char)(cur_lba & 0xFF));
        outb(ATA_PRIMARY_LBA_MID, (unsigned char)((cur_lba >> 8) & 0xFF));
        outb(ATA_PRIMARY_LBA_HI, (unsigned char)((cur_lba >> 16) & 0xFF));
        outb(ATA_PRIMARY_CMD, ATA_CMD_READ_PIO);

        if (ata_wait_drq() != 0) {
            return -3;
        }

        for (int w = 0; w < 256; w++) {
            *ptr++ = inw(ATA_PRIMARY_DATA);
        }
    }

    return 0;
}

int ata_write_sectors(uint32_t lba, uint32_t count, const void* buffer) {
    if (!buffer || count == 0) return -1;

    const uint16_t* ptr = (const uint16_t*)buffer;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t cur_lba = lba + i;

        if (ata_wait_ready() != 0) return -2;

        outb(ATA_PRIMARY_DRIVE, (unsigned char)(0xE0 | ((cur_lba >> 24) & 0x0F)));
        // 400ns delay apos selecao de drive
        inb(ATA_PRIMARY_STATUS);
        inb(ATA_PRIMARY_STATUS);
        inb(ATA_PRIMARY_STATUS);
        inb(ATA_PRIMARY_STATUS);

        if (ata_wait_ready() != 0) return -2;

        outb(ATA_PRIMARY_ERR, 0x00);
        outb(ATA_PRIMARY_SEC_CNT, 1);
        outb(ATA_PRIMARY_LBA_LO, (unsigned char)(cur_lba & 0xFF));
        outb(ATA_PRIMARY_LBA_MID, (unsigned char)((cur_lba >> 8) & 0xFF));
        outb(ATA_PRIMARY_LBA_HI, (unsigned char)((cur_lba >> 16) & 0xFF));
        outb(ATA_PRIMARY_CMD, ATA_CMD_WRITE_PIO);

        if (ata_wait_drq() != 0) {
            return -3;
        }

        for (int w = 0; w < 256; w++) {
            outw(ATA_PRIMARY_DATA, *ptr++);
        }
    }

    return 0;
}

int ata_flush(void) {
    if (!ata_available) return -1;
    if (ata_wait_ready() != 0) return -2;
    outb(ATA_PRIMARY_CMD, 0xE7); // ATA_CMD_CACHE_FLUSH
    if (ata_wait_ready() != 0) return -2;
    return 0;
}
