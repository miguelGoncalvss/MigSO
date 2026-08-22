#ifndef SHELL_SHELL_H
#define SHELL_SHELL_H

#define HISTORY_MAX 32
#define BUFFER_SIZE 128

extern volatile int matrix_running;

void shell_init(void);
void shell_update(void);
void shell_execute(const char* command);
void shell_post_command(const char* command);
int shell_has_pending_command(void);
void shell_print_prompt(void);
void shell_history_add(const char* command);
const char* shell_history_up(void);
const char* shell_history_down(void);
void shell_history_list(void);
void shell_autocomplete(char* buffer, int* cursor_pos, int* buffer_len);

#endif // SHELL_SHELL_H

