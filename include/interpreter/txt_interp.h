#ifndef INTERPRETER_TXT_INTERP_H
#define INTERPRETER_TXT_INTERP_H

#include <libc/stdint.h>
#include <libc/string.h>

#define SCRIPT_MAX_VARS     32
#define SCRIPT_VAR_NAME_LEN 32
#define SCRIPT_VAR_VAL_LEN  128

typedef struct {
    char name[SCRIPT_VAR_NAME_LEN];
    char value[SCRIPT_VAR_VAL_LEN];
    int in_use;
} script_var_t;

// Executa um arquivo .txt armazenado no MIGFS
int script_run_file(const char* filename);

// Executa um buffer de texto contendo linhas de comandos / script
int script_run_buffer(const char* text);

// Executa um buffer de script capturando a saida em string para a GUI
int script_run_buffer_capture(const char* text, char* out_buf, size_t out_size);

// Avalia uma expressao matematica (ex: "10 + 20 * 3")
int script_eval_expr(const char* expr, int* result);

// Gerenciamento de variaveis de script
void        script_set_var(const char* name, const char* value);
const char* script_get_var(const char* name);
void        script_clear_vars(void);

// Substitui $VAR nas linhas pelo valor correspondente
void        script_expand_vars(const char* src, char* dest, size_t dest_size);

#endif // INTERPRETER_TXT_INTERP_H
