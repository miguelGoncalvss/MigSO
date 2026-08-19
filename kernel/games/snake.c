#include <games/snake.h>
#include <drivers/vga.h>
#include <drivers/keyboard.h>
#include <arch/i386/timer.h>
#include <libc/stdlib.h>
#include <libc/string.h>

#define BOARD_WIDTH   60
#define BOARD_HEIGHT  24
#define OFFSET_X      10
#define OFFSET_Y      2

#define MAX_SNAKE_LEN (BOARD_WIDTH * BOARD_HEIGHT)

#define DIR_UP    0
#define DIR_DOWN  1
#define DIR_LEFT  2
#define DIR_RIGHT 3

// Constantes de teclas
#define KEY_UP_ARROW    0xad
#define KEY_DOWN_ARROW  0xaf
#define KEY_LEFT_ARROW  0xac
#define KEY_RIGHT_ARROW 0xae
#define KEY_ESCAPE      27
#define KEY_ENTER       13
#define KEY_SPACE       ' '

static int high_score = 0;

static unsigned int pseudo_rand_state = 123456789;
static unsigned int snake_rand(void) {
    pseudo_rand_state = pseudo_rand_state * 1103515245 + 12345;
    return (pseudo_rand_state >> 16) & 0x7FFF;
}

static inline void draw_char(int x, int y, char c, unsigned char fg, unsigned char bg) {
    vga_set_cell(x, y, c, fg, bg);
}

static void draw_string(int x, int y, const char* str, unsigned char fg, unsigned char bg) {
    for (int i = 0; str[i] != '\0' && (x + i) < VGA_WIDTH; i++) {
        draw_char(x + i, y, str[i], fg, bg);
    }
}

static void draw_number(int x, int y, int num, unsigned char fg, unsigned char bg) {
    char buf[16];
    itoa(num, buf, 10);
    draw_string(x, y, buf, fg, bg);
}

static void draw_arena_border(void) {
    // Top border
    draw_char(OFFSET_X - 1, OFFSET_Y - 1, '+', VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    for (int x = 0; x < BOARD_WIDTH; x++) {
        draw_char(OFFSET_X + x, OFFSET_Y - 1, '-', VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    }
    draw_char(OFFSET_X + BOARD_WIDTH, OFFSET_Y - 1, '+', VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);

    // Side borders
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        draw_char(OFFSET_X - 1, OFFSET_Y + y, '|', VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        draw_char(OFFSET_X + BOARD_WIDTH, OFFSET_Y + y, '|', VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    }

    // Bottom border
    draw_char(OFFSET_X - 1, OFFSET_Y + BOARD_HEIGHT, '+', VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    for (int x = 0; x < BOARD_WIDTH; x++) {
        draw_char(OFFSET_X + x, OFFSET_Y + BOARD_HEIGHT, '-', VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    }
    draw_char(OFFSET_X + BOARD_WIDTH, OFFSET_Y + BOARD_HEIGHT, '+', VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
}

void snake_game_main(void) {
    vga_clear();
    keyboard_set_doom_mode(1); // Ativa modo de captura de eventos diretos

    int play_again = 1;

    while (play_again) {
        vga_clear();

        // Inicializa semente pseudo-aleatoria com o timer
        pseudo_rand_state ^= timer_get_ticks();

        // Estado da Cobra
        int snake_x[MAX_SNAKE_LEN];
        int snake_y[MAX_SNAKE_LEN];
        int snake_len = 4;
        int dir = DIR_RIGHT;
        int next_dir = DIR_RIGHT;
        int score = 0;
        int level = 1;
        int delay_ms = 110;
        int paused = 0;
        int game_over = 0;

        // Posi??o inicial da cobra no centro
        int start_x = BOARD_WIDTH / 4;
        int start_y = BOARD_HEIGHT / 2;
        for (int i = 0; i < snake_len; i++) {
            snake_x[i] = start_x - i;
            snake_y[i] = start_y;
        }

        // Posi??o da comida
        int food_x = BOARD_WIDTH / 2;
        int food_y = BOARD_HEIGHT / 2;

        // Bonus fruit ($)
        int bonus_x = -1;
        int bonus_y = -1;
        int bonus_timer = 0;

        // Desenha moldura e cabecalho
        draw_arena_border();
        draw_string(2, 0, "=== migOS Snake Game ===", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        draw_string(30, 0, "Score: ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        draw_number(37, 0, score, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        draw_string(45, 0, "Recorde: ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        draw_number(54, 0, high_score, VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
        draw_string(64, 0, "Nivel: ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        draw_number(71, 0, level, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);

        draw_string(6, 23, "[WASD / Setas] Mover   [Espaco/P] Pausa   [Q/ESC] Sair", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);

        // Desenha a comida inicial
        draw_char(OFFSET_X + food_x, OFFSET_Y + food_y, '*', VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

        while (!game_over) {
            // Processa teclas da fila
            int pressed;
            unsigned char key;
            while (keyboard_get_doom_key(&pressed, &key)) {
                if (!pressed) continue;

                if (key == 'w' || key == 'W' || key == KEY_UP_ARROW) {
                    if (dir != DIR_DOWN) next_dir = DIR_UP;
                } else if (key == 's' || key == 'S' || key == KEY_DOWN_ARROW) {
                    if (dir != DIR_UP) next_dir = DIR_DOWN;
                } else if (key == 'a' || key == 'A' || key == KEY_LEFT_ARROW) {
                    if (dir != DIR_RIGHT) next_dir = DIR_LEFT;
                } else if (key == 'd' || key == 'D' || key == KEY_RIGHT_ARROW) {
                    if (dir != DIR_LEFT) next_dir = DIR_RIGHT;
                } else if (key == KEY_SPACE || key == 'p' || key == 'P') {
                    paused = !paused;
                    if (paused) {
                        draw_string(OFFSET_X + 18, OFFSET_Y + 9, " [ JOGO PAUSADO ] ", VGA_COLOR_YELLOW, VGA_COLOR_BLUE);
                    } else {
                        draw_string(OFFSET_X + 18, OFFSET_Y + 9, "                  ", VGA_COLOR_BLACK, VGA_COLOR_BLACK);
                    }
                } else if (key == 'q' || key == 'Q' || key == KEY_ESCAPE) {
                    game_over = 1;
                    play_again = 0;
                    break;
                }
            }

            if (game_over) break;

            if (paused) {
                sleep(50);
                continue;
            }

            dir = next_dir;

            // Calcula nova cabeca
            int new_head_x = snake_x[0];
            int new_head_y = snake_y[0];

            if (dir == DIR_UP)         new_head_y--;
            else if (dir == DIR_DOWN)  new_head_y++;
            else if (dir == DIR_LEFT)  new_head_x--;
            else if (dir == DIR_RIGHT) new_head_x++;

            // Checagem de colisao com paredes
            if (new_head_x < 0 || new_head_x >= BOARD_WIDTH ||
                new_head_y < 0 || new_head_y >= BOARD_HEIGHT) {
                game_over = 1;
                break;
            }

            // Checagem de colisao com o proprio corpo
            for (int i = 0; i < snake_len; i++) {
                if (snake_x[i] == new_head_x && snake_y[i] == new_head_y) {
                    game_over = 1;
                    break;
                }
            }
            if (game_over) break;

            // Verifica se comeu a comida regular
            int ate_food = (new_head_x == food_x && new_head_y == food_y);
            int ate_bonus = (bonus_x >= 0 && new_head_x == bonus_x && new_head_y == bonus_y);

            if (ate_food) {
                score += 10;
                if (snake_len < MAX_SNAKE_LEN - 1) {
                    snake_len++;
                }

                // Aumenta velocidade a cada 50 pontos
                level = 1 + (score / 50);
                delay_ms = 110 - (level * 8);
                if (delay_ms < 40) delay_ms = 40;

                // Gera nova comida em local livre
                int valid = 0;
                while (!valid) {
                    food_x = snake_rand() % BOARD_WIDTH;
                    food_y = snake_rand() % BOARD_HEIGHT;
                    valid = 1;
                    for (int i = 0; i < snake_len; i++) {
                        if (snake_x[i] == food_x && snake_y[i] == food_y) {
                            valid = 0;
                            break;
                        }
                    }
                }

                // Chance de gerar bonus ($)
                if (bonus_x < 0 && (snake_rand() % 4 == 0)) {
                    bonus_x = snake_rand() % BOARD_WIDTH;
                    bonus_y = snake_rand() % BOARD_HEIGHT;
                    bonus_timer = 60; // Dura 60 frames
                }
            }

            if (ate_bonus) {
                score += 50;
                bonus_x = -1;
                bonus_y = -1;
                bonus_timer = 0;
            }

            // Limpa o rabo anterior da tela se nao cresceu
            if (!ate_food) {
                int tail_x = snake_x[snake_len - 1];
                int tail_y = snake_y[snake_len - 1];
                draw_char(OFFSET_X + tail_x, OFFSET_Y + tail_y, ' ', VGA_COLOR_BLACK, VGA_COLOR_BLACK);
            }

            // Move os segmentos do corpo
            for (int i = snake_len - 1; i > 0; i--) {
                snake_x[i] = snake_x[i - 1];
                snake_y[i] = snake_y[i - 1];
            }
            snake_x[0] = new_head_x;
            snake_y[0] = new_head_y;

            // Desenha o corpo da cobra
            for (int i = 1; i < snake_len; i++) {
                draw_char(OFFSET_X + snake_x[i], OFFSET_Y + snake_y[i], 'o', VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            }

            // Desenha a cabeca da cobra
            draw_char(OFFSET_X + snake_x[0], OFFSET_Y + snake_y[0], '@', VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);

            // Desenha a comida
            draw_char(OFFSET_X + food_x, OFFSET_Y + food_y, '*', VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

            // Atualiza e desenha bonus
            if (bonus_x >= 0) {
                bonus_timer--;
                if (bonus_timer <= 0) {
                    draw_char(OFFSET_X + bonus_x, OFFSET_Y + bonus_y, ' ', VGA_COLOR_BLACK, VGA_COLOR_BLACK);
                    bonus_x = -1;
                    bonus_y = -1;
                } else {
                    draw_char(OFFSET_X + bonus_x, OFFSET_Y + bonus_y, '$', VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
                }
            }

            // Atualiza placar
            draw_number(37, 0, score, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            if (score > high_score) {
                high_score = score;
                draw_number(54, 0, high_score, VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
            }
            draw_number(71, 0, level, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);

            sleep(delay_ms);
        }

        if (play_again) {
            // Exibe janela de Game Over
            int box_x = OFFSET_X + 6;
            int box_y = OFFSET_Y + 5;
            int box_w = 38;
            int box_h = 9;

            for (int y = 0; y < box_h; y++) {
                for (int x = 0; x < box_w; x++) {
                    draw_char(box_x + x, box_y + y, ' ', VGA_COLOR_WHITE, VGA_COLOR_BLUE);
                }
            }

            draw_string(box_x + 13, box_y + 1, "*** GAME OVER ***", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLUE);
            draw_string(box_x + 8, box_y + 3, "Pontuacao Final: ", VGA_COLOR_WHITE, VGA_COLOR_BLUE);
            draw_number(box_x + 25, box_y + 3, score, VGA_COLOR_YELLOW, VGA_COLOR_BLUE);

            draw_string(box_x + 8, box_y + 4, "Recorde Atual:   ", VGA_COLOR_WHITE, VGA_COLOR_BLUE);
            draw_number(box_x + 25, box_y + 4, high_score, VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLUE);

            draw_string(box_x + 3, box_y + 6, "[R] Jogar Novamente  |  [Q] Sair", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLUE);

            // Aguarda resposta do jogador
            int waiting = 1;
            while (waiting) {
                int pressed;
                unsigned char key;
                if (keyboard_get_doom_key(&pressed, &key) && pressed) {
                    if (key == 'r' || key == 'R' || key == KEY_ENTER) {
                        play_again = 1;
                        waiting = 0;
                    } else if (key == 'q' || key == 'Q' || key == KEY_ESCAPE) {
                        play_again = 0;
                        waiting = 0;
                    }
                }
                sleep(30);
            }
        }
    }

    keyboard_set_doom_mode(0); // Restaura modo normal do teclado para o shell
    vga_clear();
}
