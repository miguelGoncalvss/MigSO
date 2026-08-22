#include <drivers/sound.h>
#include <arch/i386/io.h>
#include <arch/i386/timer.h>

static int sound_muted = 0;
static int sound_initialized = 0;

// Tabelas estáticas de notas para efeitos sonoros (SFX)

// 1. Mac OS Classic Startup Chime (Acorde arpejado Fá Maior: F3 -> C4 -> F4 -> A4 -> C5)
static const note_t sfx_startup[] = {
    { 175, 25 },  // F3  (175 Hz, 25 ms)
    { 262, 25 },  // C4  (262 Hz, 25 ms)
    { 349, 30 },  // F4  (349 Hz, 30 ms)
    { 440, 40 },  // A4  (440 Hz, 40 ms)
    { 523, 220 }  // C5  (523 Hz, 220 ms)
};

// 2. Beep de Alerta da GUI / Sosumi (A5 880Hz -> Pausa 10ms -> F5 698Hz)
static const note_t sfx_alert[] = {
    { 880, 30 },  // A5  (880 Hz, 30 ms)
    { 0,   10 },  // Silêncio / Pausa (10 ms)
    { 698, 60 }   // F5  (698 Hz, 60 ms)
};

// 3. Clique de Interface de Usuário (Pulso ultra-curto de 2500 Hz)
static const note_t sfx_click[] = {
    { 2500, 2 }   // 2500 Hz por 2 ms
};

// 4. Esvaziamento de Lixeira / Ação Destrutiva (Varredura descendente 800Hz -> 150Hz)
static const note_t sfx_trash[] = {
    { 800, 15 },  // 800 Hz (15 ms)
    { 500, 15 },  // 500 Hz (15 ms)
    { 300, 25 },  // 300 Hz (25 ms)
    { 150, 40 }   // 150 Hz (40 ms)
};

// 5. Confirmação de Sucesso / Save Game (G5 784Hz -> C6 1047Hz)
static const note_t sfx_success[] = {
    { 784,  40 },  // G5 (784 Hz, 40 ms)
    { 1047, 120 }  // C6 (1047 Hz, 120 ms)
};

void sound_init(void) {
    sound_muted = 0;
    sound_stop();
    sound_initialized = 1;
}

void sound_set_mute(int mute) {
    sound_muted = mute ? 1 : 0;
    if (sound_muted) {
        sound_stop();
    }
}

int sound_is_muted(void) {
    return sound_muted;
}

void sound_tone(uint32_t freq_hz) {
    if (sound_muted || freq_hz == 0) {
        sound_stop();
        return;
    }

    // Calcula o divisor de 16 bits do PIT baseado no oscilador de 1.193.180 Hz
    uint32_t divisor = PIT_TIMER_BASE_FREQ / freq_hz;
    if (divisor > 65535) divisor = 65535;
    if (divisor == 0) divisor = 1;

    // Configura o Canal 2 do PIT no Modo 3 (Square Wave Generator)
    outb(PIT_COMMAND_PORT, PIT_CHANNEL2_SQUARE_WAVE);
    io_wait();

    // Envia o divisor de 16 bits (primeiro byte baixo LSB, depois byte alto MSB)
    outb(PIT_CHANNEL2_PORT, (uint8_t)(divisor & 0xFF));
    io_wait();
    outb(PIT_CHANNEL2_PORT, (uint8_t)((divisor >> 8) & 0xFF));
    io_wait();

    // Habilita o alto-falante na porta 0x61
    // Bit 0 = 1 (Porta de controle do timer PIT 2 ligada ao alto-falante)
    // Bit 1 = 1 (Porta de dados do speaker ligada)
    uint8_t reg = inb(PC_SPEAKER_PORT);
    outb(PC_SPEAKER_PORT, (uint8_t)(reg | 0x03));
}

void sound_stop(void) {
    // Desativa os bits 0 e 1 da porta 0x61, silenciando o speaker
    uint8_t reg = inb(PC_SPEAKER_PORT);
    outb(PC_SPEAKER_PORT, (uint8_t)(reg & 0xFC));
}

void sound_beep(uint32_t freq_hz, uint32_t duration_ms) {
    if (sound_muted) return;

    if (freq_hz > 0) {
        sound_tone(freq_hz);
    } else {
        sound_stop();
    }

    if (duration_ms > 0) {
        sleep(duration_ms);
    }

    sound_stop();
}

void sound_play_sfx(sfx_type_t sfx) {
    if (sound_muted) return;

    switch (sfx) {
        case SFX_STARTUP:
            sound_play_melody(sfx_startup, sizeof(sfx_startup) / sizeof(sfx_startup[0]));
            break;

        case SFX_ALERT:
            sound_play_melody(sfx_alert, sizeof(sfx_alert) / sizeof(sfx_alert[0]));
            break;

        case SFX_CLICK: {
            sound_tone(2500);
            sleep(10);
            sound_stop();
            break;
        }

        case SFX_TRASH:
            sound_play_melody(sfx_trash, sizeof(sfx_trash) / sizeof(sfx_trash[0]));
            break;

        case SFX_SUCCESS:
            sound_play_melody(sfx_success, sizeof(sfx_success) / sizeof(sfx_success[0]));
            break;

        default:
            break;
    }
}

void sound_play_melody(const note_t* notes, uint32_t count) {
    if (sound_muted || !notes || count == 0) return;

    for (uint32_t i = 0; i < count; i++) {
        if (sound_muted) {
            sound_stop();
            return;
        }

        if (notes[i].freq_hz > 0) {
            sound_tone(notes[i].freq_hz);
        } else {
            sound_stop();
        }

        if (notes[i].duration_ms > 0) {
            sleep(notes[i].duration_ms);
        }

        // Breve pausa para definicao e clareza acústica entre notas adjacentes
        if (i + 1 < count && notes[i].freq_hz > 0 && notes[i + 1].freq_hz > 0) {
            sound_stop();
            sleep(5);
        }
    }

    sound_stop();
}
