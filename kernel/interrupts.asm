[bits 32]

; Importa a função C (com o underscore padrão do GCC de 32-bit)
extern _keyboard_handler_c

; Exporta os dois formatos de símbolo para garantir resolução universal no linker
global _keyboard_isr
global keyboard_isr

_keyboard_isr:
keyboard_isr:
    pusha

    call _keyboard_handler_c

    popa
    iretd