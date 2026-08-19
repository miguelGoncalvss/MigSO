#include <drivers/bga.h>
#include <arch/i386/io.h>
#include <libc/string.h>

#define VBE_DISPI_IOPORT_INDEX          0x01CE
#define VBE_DISPI_IOPORT_DATA           0x01CF

#define VBE_DISPI_INDEX_ID              0x0
#define VBE_DISPI_INDEX_XRES            0x1
#define VBE_DISPI_INDEX_YRES            0x2
#define VBE_DISPI_INDEX_BPP             0x3
#define VBE_DISPI_INDEX_ENABLE          0x4
#define VBE_DISPI_INDEX_BANK            0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH      0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT     0x7
#define VBE_DISPI_INDEX_X_OFFSET        0x8
#define VBE_DISPI_INDEX_Y_OFFSET        0x9

#define VBE_DISPI_DISABLED              0x00
#define VBE_DISPI_ENABLED               0x01
#define VBE_DISPI_LFB_ENABLED           0x40
#define VBE_DISPI_NOCLEARMEM            0x80

static uint32_t lfb_physical_address = 0xFD000000;
static uint32_t* lfb_pointer = (uint32_t*)0xFD000000;
static int bga_initialized = 0;

static void bga_write_register(uint16_t index, uint16_t value) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, value);
}

static uint16_t bga_read_register(uint16_t index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

static uint32_t pci_read_config_32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC));
    outl(0xCF8, address);
    return inl(0xCFC);
}

static uint32_t detect_pci_lfb_address(void) {
    for (uint8_t bus = 0; bus < 8; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t dev_ven = pci_read_config_32(bus, slot, 0, 0x00);
            if (dev_ven == 0xFFFFFFFF || dev_ven == 0) continue;

            uint32_t class_rev = pci_read_config_32(bus, slot, 0, 0x08);
            uint8_t base_class = (uint8_t)((class_rev >> 24) & 0xFF);

            // Classe 0x03 = Display Controller ou Vendor 0x1234 = Bochs/QEMU VGA
            if (base_class == 0x03 || (dev_ven & 0xFFFF) == 0x1234) {
                uint32_t bar0 = pci_read_config_32(bus, slot, 0, 0x10);
                if (bar0 != 0 && bar0 != 0xFFFFFFFF) {
                    return (bar0 & 0xFFFFFFF0);
                }
            }
        }
    }
    return 0xFD000000; // Fallback padrao QEMU
}

int bga_is_available(void) {
    uint16_t id = bga_read_register(VBE_DISPI_INDEX_ID);
    return (id >= 0xB0C0 && id <= 0xB0C6);
}

int bga_init(void) {
    if (!bga_is_available()) {
        return 0;
    }

    lfb_physical_address = detect_pci_lfb_address();
    lfb_pointer = (uint32_t*)lfb_physical_address;

    bga_set_video_mode(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP);
    bga_clear(0x00000000);
    bga_initialized = 1;
    return 1;
}

uint32_t bga_get_lfb_address(void) {
    return lfb_physical_address;
}

uint32_t* bga_get_framebuffer(void) {
    return lfb_pointer;
}

void bga_set_video_mode(uint16_t width, uint16_t height, uint16_t bpp) {
    bga_write_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    bga_write_register(VBE_DISPI_INDEX_XRES, width);
    bga_write_register(VBE_DISPI_INDEX_YRES, height);
    bga_write_register(VBE_DISPI_INDEX_BPP, bpp);
    bga_write_register(VBE_DISPI_INDEX_VIRT_WIDTH, width);
    bga_write_register(VBE_DISPI_INDEX_VIRT_HEIGHT, height);
    bga_write_register(VBE_DISPI_INDEX_X_OFFSET, 0);
    bga_write_register(VBE_DISPI_INDEX_Y_OFFSET, 0);
    bga_write_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
}

void bga_disable(void) {
    bga_write_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    bga_initialized = 0;
}

void bga_putpixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return;
    lfb_pointer[y * SCREEN_WIDTH + x] = color;
}

void bga_clear(uint32_t color) {
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        lfb_pointer[i] = color;
    }
}

void bga_blit(const uint32_t* buffer) {
    if (!buffer) return;
    memcpy(lfb_pointer, buffer, SCREEN_SIZE_BYTES);
}
