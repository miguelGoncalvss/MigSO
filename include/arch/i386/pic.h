#ifndef ARCH_I386_PIC_H
#define ARCH_I386_PIC_H

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

void pic_remap(void);
void pic_send_eoi(unsigned char irq);
void pic_unmask_irq(unsigned char irq);
void pic_mask_irq(unsigned char irq);

#endif // ARCH_I386_PIC_H
