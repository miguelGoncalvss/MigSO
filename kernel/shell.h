#ifndef SHELL_H
#define SHELL_H

#define HISTORY_MAX 8
#define BUFFER_SIZE 128

extern volatile int matrix_running;

void shell_init(void);
void shell_execute(const char* command);
void shell_history_add(const char* command);
const char* shell_history_up(void);
const char* shell_history_down(void);

#endif