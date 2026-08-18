ENTRY(_start)
SECTIONS
{
    . = 0x10000;
    .text : { *(.text*) }
    .data : { *(.data*) }
    .rdata : { *(.rdata*) }
    .bss : { *(.bss*) }
    /DISCARD/ : { *(.reloc*) *(.pdata*) *(.xdata*) *(.eh_frame*) *(.comment) }
}