ENTRY(_start)
SECTIONS
{
    . = 0x10000;
    .text : { *(.text*) }
    .data : { *(.data*) }
    .rdata : { *(.rdata*) }
    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }
    /DISCARD/ : { *(.reloc*) *(.pdata*) *(.xdata*) *(.eh_frame*) *(.comment) }
}