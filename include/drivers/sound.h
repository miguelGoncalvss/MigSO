#ifndef DRIVERS_SOUND_H
#define DRIVERS_SOUND_H

#include <libc/stdint.h>
#include <libc/stdbool.h>

// Portas de I/O do PC Speaker e PIT 8254
#define PC_SPEAKER_PORT       0x61
#define PIT_CHANNEL2_PORT     0x42
#define PIT_COMMAND_PORT      0x43

// Comando do PIT: Canal 2, Acesso LSB/MSB, Modo 3 (Square Wave Generator), Contador Binario de 16 bits (0xB6)
#define PIT_CHANNEL2_SQUARE_WAVE 0xB6

// Frequencia base do oscilador do PIT 8254 (1.193.180 Hz)
#define PIT_TIMER_BASE_FREQ   1193180

// Frequências das notas musicais padrao (em Hz)
#define NOTE_C3   131
#define NOTE_D3   147
#define NOTE_E3   165
#define NOTE_F3   175
#define NOTE_G3   196
#define NOTE_A3   220
#define NOTE_B3   247

#define NOTE_C4   262
#define NOTE_D4   294
#define NOTE_E4   330
#define NOTE_F4   349
#define NOTE_G4   392
#define NOTE_A4   440
#define NOTE_B4   494

#define NOTE_C5   523
#define NOTE_D5   587
#define NOTE_E5   659
#define NOTE_F5   698
#define NOTE_G5   784
#define NOTE_A5   880
#define NOTE_B5   988

#define NOTE_C6   1047
#define NOTE_D6   1175
#define NOTE_E6   1319
#define NOTE_F6   1397
#define NOTE_G6   1568
#define NOTE_A6   1760
#define NOTE_B6   1976

// Estrutura de Nota Musical (Frequência em Hz e Duração em milissegundos)
typedef struct {
    uint32_t freq_hz;     // Frequência em Hertz (0 = Silêncio/Pausa)
    uint32_t duration_ms; // Duração em milissegundos
} note_t;

// Tipos de Efeitos Sonoros do Sistema (SFX)
typedef enum {
    SFX_STARTUP,  // Mac OS Classic Startup Chime (Acorde arpejado Fá Maior: F3 -> C4 -> F4 -> A4 -> C5)
    SFX_ALERT,    // Beep de Alerta da GUI / Sosumi (A5 880Hz -> Pausa -> F5 698Hz)
    SFX_CLICK,    // Clique de Interface de Usuário (Pulso curto 2500Hz)
    SFX_TRASH,    // Esvaziamento de Lixeira / Ação Destrutiva (Varredura descendente 800Hz -> 150Hz)
    SFX_SUCCESS   // Confirmação de Sucesso / Save Game (G5 784Hz -> C6 1047Hz)
} sfx_type_t;

// Inicializa o driver de som do PC Speaker
void sound_init(void);

// Controle de Silenciamento (Mudo)
void sound_set_mute(int mute);
int  sound_is_muted(void);

// Toca uma frequência contínua em Hz (não bloqueante)
void sound_tone(uint32_t freq_hz);

// Interrompe qualquer emissao de som do PC Speaker
void sound_stop(void);

// Emite um beep simples de determinada frequência e duração (bloqueante)
void sound_beep(uint32_t freq_hz, uint32_t duration_ms);

// Reproduz um efeito sonoro pré-definido do sistema (bloqueante)
void sound_play_sfx(sfx_type_t sfx);

// Reproduz uma melodia ou sequência de notas
void sound_play_melody(const note_t* notes, uint32_t count);

#endif // DRIVERS_SOUND_H
