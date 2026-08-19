#include <interpreter/txt_interp.h>
#include <fs/migfs.h>
#include <shell/shell.h>
#include <drivers/vga.h>
#include <drivers/keyboard.h>
#include <arch/i386/timer.h>
#include <libc/stdio.h>
#include <libc/stdlib.h>
#include <libc/string.h>

static script_var_t var_table[SCRIPT_MAX_VARS];

void script_clear_vars(void) {
    memset(var_table, 0, sizeof(var_table));
}

void script_set_var(const char* name, const char* value) {
    if (!name || name[0] == '\0') return;

    // Se a variavel ja existe, atualiza
    for (int i = 0; i < SCRIPT_MAX_VARS; i++) {
        if (var_table[i].in_use && strcmp(var_table[i].name, name) == 0) {
            strncpy(var_table[i].value, value ? value : "", SCRIPT_VAR_VAL_LEN - 1);
            var_table[i].value[SCRIPT_VAR_VAL_LEN - 1] = '\0';
            return;
        }
    }

    // Cria nova variavel
    for (int i = 0; i < SCRIPT_MAX_VARS; i++) {
        if (!var_table[i].in_use) {
            strncpy(var_table[i].name, name, SCRIPT_VAR_NAME_LEN - 1);
            var_table[i].name[SCRIPT_VAR_NAME_LEN - 1] = '\0';
            strncpy(var_table[i].value, value ? value : "", SCRIPT_VAR_VAL_LEN - 1);
            var_table[i].value[SCRIPT_VAR_VAL_LEN - 1] = '\0';
            var_table[i].in_use = 1;
            return;
        }
    }
}

const char* script_get_var(const char* name) {
    if (!name) return NULL;
    for (int i = 0; i < SCRIPT_MAX_VARS; i++) {
        if (var_table[i].in_use && strcmp(var_table[i].name, name) == 0) {
            return var_table[i].value;
        }
    }
    return NULL;
}

void script_expand_vars(const char* src, char* dest, size_t dest_size) {
    if (!src || !dest || dest_size == 0) return;

    size_t s = 0;
    size_t d = 0;

    while (src[s] != '\0' && d < dest_size - 1) {
        if (src[s] == '$' && ((src[s+1] >= 'a' && src[s+1] <= 'z') || (src[s+1] >= 'A' && src[s+1] <= 'Z') || src[s+1] == '_')) {
            s++; // Pula o '$'
            char var_name[SCRIPT_VAR_NAME_LEN];
            size_t vn = 0;
            while (( (src[s] >= 'a' && src[s] <= 'z') || 
                     (src[s] >= 'A' && src[s] <= 'Z') || 
                     (src[s] >= '0' && src[s] <= '9') || 
                     src[s] == '_' ) && vn < SCRIPT_VAR_NAME_LEN - 1) {
                var_name[vn++] = src[s++];
            }
            var_name[vn] = '\0';

            const char* val = script_get_var(var_name);
            if (val) {
                while (*val && d < dest_size - 1) {
                    dest[d++] = *val++;
                }
            }
        } else {
            dest[d++] = src[s++];
        }
    }
    dest[d] = '\0';
}

// ============================================================
// Avaliador de Expressoes Aritmeticas (Recursive Descent)
// ============================================================

typedef struct {
    const char* str;
    int pos;
} parser_t;

static void skip_ws(parser_t* p) {
    while (p->str[p->pos] == ' ' || p->str[p->pos] == '\t') {
        p->pos++;
    }
}

static int parse_expr(parser_t* p);

static int parse_factor(parser_t* p) {
    skip_ws(p);

    if (p->str[p->pos] == '(') {
        p->pos++;
        int val = parse_expr(p);
        skip_ws(p);
        if (p->str[p->pos] == ')') {
            p->pos++;
        }
        return val;
    }

    if (p->str[p->pos] == '-') {
        p->pos++;
        return -parse_factor(p);
    }

    if (p->str[p->pos] == '+') {
        p->pos++;
        return parse_factor(p);
    }

    if (p->str[p->pos] == '$' || (p->str[p->pos] >= 'A' && p->str[p->pos] <= 'Z') || (p->str[p->pos] >= 'a' && p->str[p->pos] <= 'z')) {
        char vname[32];
        int vi = 0;
        if (p->str[p->pos] == '$') p->pos++;
        while (((p->str[p->pos] >= 'a' && p->str[p->pos] <= 'z') || 
                (p->str[p->pos] >= 'A' && p->str[p->pos] <= 'Z') || 
                (p->str[p->pos] >= '0' && p->str[p->pos] <= '9') || 
                p->str[p->pos] == '_') && vi < 31) {
            vname[vi++] = p->str[p->pos++];
        }
        vname[vi] = '\0';
        const char* val = script_get_var(vname);
        return val ? atoi(val) : 0;
    }

    int val = 0;
    while (p->str[p->pos] >= '0' && p->str[p->pos] <= '9') {
        val = val * 10 + (p->str[p->pos] - '0');
        p->pos++;
    }
    return val;
}

static int parse_term(parser_t* p) {
    int val = parse_factor(p);
    skip_ws(p);

    while (p->str[p->pos] == '*' || p->str[p->pos] == '/' || p->str[p->pos] == '%') {
        char op = p->str[p->pos++];
        int right = parse_factor(p);
        if (op == '*') {
            val *= right;
        } else if (op == '/') {
            if (right != 0) val /= right;
            else val = 0;
        } else if (op == '%') {
            if (right != 0) val %= right;
            else val = 0;
        }
        skip_ws(p);
    }
    return val;
}

static int parse_expr(parser_t* p) {
    int val = parse_term(p);
    skip_ws(p);

    while (p->str[p->pos] == '+' || p->str[p->pos] == '-') {
        char op = p->str[p->pos++];
        int right = parse_term(p);
        if (op == '+') {
            val += right;
        } else if (op == '-') {
            val -= right;
        }
        skip_ws(p);
    }
    return val;
}

int script_eval_expr(const char* expr, int* result) {
    if (!expr || !result) return -1;
    parser_t p;
    p.str = expr;
    p.pos = 0;
    *result = parse_expr(&p);
    return 0;
}

// ============================================================
// Execucao de Linha e Buffer de Script
// ============================================================

static void script_run_line(const char* raw_line) {
    while (*raw_line == ' ' || *raw_line == '\t') raw_line++;
    if (*raw_line == '\0') return;

    // Comentarios
    if (*raw_line == '#' || (*raw_line == '/' && *(raw_line+1) == '/') || *raw_line == ';') {
        return;
    }

    char expanded[512];
    script_expand_vars(raw_line, expanded, sizeof(expanded));

    char* line = expanded;
    while (*line == ' ' || *line == '\t') line++;

    // Comando 'echo' ou 'print'
    if (strncmp(line, "echo.", 5) == 0) {
        vga_putc('\n');
        return;
    }
    if (strncmp(line, "echo", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) {
        const char* msg = line + 4;
        while (*msg == ' ') msg++;
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_puts(msg);
        vga_putc('\n');
        return;
    }
    if (strncmp(line, "print", 5) == 0 && (line[5] == ' ' || line[5] == '\0')) {
        const char* msg = line + 5;
        while (*msg == ' ') msg++;
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_puts(msg);
        vga_putc('\n');
        return;
    }

    // Comando 'set VAR=VALOR'
    if (strncmp(line, "set", 3) == 0 && line[3] == ' ') {
        const char* p = line + 4;
        while (*p == ' ') p++;
        char var_name[SCRIPT_VAR_NAME_LEN];
        int vi = 0;
        while (*p != '=' && *p != ' ' && *p != '\0' && vi < SCRIPT_VAR_NAME_LEN - 1) {
            var_name[vi++] = *p++;
        }
        var_name[vi] = '\0';
        while (*p == ' ' || *p == '=') p++;
        script_set_var(var_name, p);
        return;
    }

    // Comando 'calc' ou 'eval'
    if ((strncmp(line, "calc", 4) == 0 && line[4] == ' ') || (strncmp(line, "eval", 4) == 0 && line[4] == ' ')) {
        const char* expr = (line[0] == 'c') ? line + 5 : line + 5;
        while (*expr == ' ') expr++;
        int result = 0;
        if (script_eval_expr(expr, &result) == 0) {
            char buf[32];
            itoa(result, buf, 10);
            vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
            vga_puts("[CALC] ");
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            vga_puts(expr);
            vga_puts(" = ");
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            vga_puts(buf);
            vga_putc('\n');
            // Armazena automaticamente em variavel especial $RESULT
            script_set_var("RESULT", buf);
        }
        return;
    }

    // Comando 'sleep <ms>'
    if (strncmp(line, "sleep", 5) == 0 && line[5] == ' ') {
        int ms = atoi(line + 6);
        if (ms > 0) sleep(ms);
        return;
    }

    // Comando 'pause'
    if (strcmp(line, "pause") == 0) {
        vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        vga_puts("Pressione qualquer tecla para continuar...\n");
        keyboard_clear_key();
        while (!keyboard_has_key()) {
            __asm__ volatile ("hlt");
        }
        keyboard_clear_key();
        return;
    }

    // Comando 'color <cor>'
    if (strncmp(line, "color", 5) == 0 && line[5] == ' ') {
        const char* col = line + 6;
        while (*col == ' ') col++;
        if (strcmp(col, "green") == 0) vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        else if (strcmp(col, "cyan") == 0) vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        else if (strcmp(col, "red") == 0) vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        else if (strcmp(col, "yellow") == 0) vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        else if (strcmp(col, "white") == 0) vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        return;
    }

    // Executa comando no Shell padrao do SO
    shell_execute(line);
}

int script_run_buffer(const char* text) {
    if (!text) return -1;

    char line_buf[512];
    size_t i = 0;
    size_t li = 0;

    while (text[i] != '\0') {
        char c = text[i++];
        if (c == '\r') continue;
        if (c == '\n') {
            line_buf[li] = '\0';
            script_run_line(line_buf);
            li = 0;
        } else {
            if (li < sizeof(line_buf) - 1) {
                line_buf[li++] = c;
            }
        }
    }

    if (li > 0) {
        line_buf[li] = '\0';
        script_run_line(line_buf);
    }

    return 0;
}

int script_run_file(const char* filename) {
    if (!filename || filename[0] == '\0') {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Uso: run <arquivo.txt>\n");
        return -1;
    }

    migfs_file_t* f = migfs_open(filename);
    if (!f || !f->data) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Interpretador: arquivo '");
        vga_puts(filename);
        vga_puts("' nao encontrado no disco!\n");
        return -2;
    }

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("--- [Executando: ");
    vga_puts(filename);
    vga_puts("] ---\n");

    int ret = script_run_buffer(f->data);

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("--- [Fim do Script: ");
    vga_puts(filename);
    vga_puts("] ---\n");

    return ret;
}

static void append_to_output(char* out, size_t max, const char* str) {
    size_t cur = strlen(out);
    size_t add = strlen(str);
    if (cur + add < max - 1) {
        strcpy(out + cur, str);
    }
}

int script_run_buffer_capture(const char* text, char* out_buf, size_t out_size) {
    if (!text || !out_buf || out_size == 0) return -1;
    out_buf[0] = '\0';

    char line_buf[512];
    size_t i = 0;
    size_t li = 0;

    while (text[i] != '\0') {
        char c = text[i++];
        if (c == '\r') continue;
        if (c == '\n') {
            line_buf[li] = '\0';
            li = 0;

            char expanded[512];
            script_expand_vars(line_buf, expanded, sizeof(expanded));
            char* line = expanded;
            while (*line == ' ' || *line == '\t') line++;

            if (*line == '#' || (*line == '/' && *(line+1) == '/') || *line == ';' || *line == '\0') {
                continue;
            }

            if (strncmp(line, "echo.", 5) == 0) {
                append_to_output(out_buf, out_size, "\n");
            } else if (strncmp(line, "echo", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) {
                const char* msg = line + 4;
                while (*msg == ' ') msg++;
                append_to_output(out_buf, out_size, msg);
                append_to_output(out_buf, out_size, "\n");
            } else if (strncmp(line, "set", 3) == 0 && line[3] == ' ') {
                const char* p = line + 4;
                while (*p == ' ') p++;
                char var_name[SCRIPT_VAR_NAME_LEN];
                int vi = 0;
                while (*p != '=' && *p != ' ' && *p != '\0' && vi < SCRIPT_VAR_NAME_LEN - 1) {
                    var_name[vi++] = *p++;
                }
                var_name[vi] = '\0';
                while (*p == ' ' || *p == '=') p++;
                script_set_var(var_name, p);
                append_to_output(out_buf, out_size, "[SET] ");
                append_to_output(out_buf, out_size, var_name);
                append_to_output(out_buf, out_size, " = ");
                append_to_output(out_buf, out_size, p);
                append_to_output(out_buf, out_size, "\n");
            } else if ((strncmp(line, "calc", 4) == 0 && line[4] == ' ') || (strncmp(line, "eval", 4) == 0 && line[4] == ' ')) {
                const char* expr = line + 5;
                while (*expr == ' ') expr++;
                int result = 0;
                if (script_eval_expr(expr, &result) == 0) {
                    char res_buf[64];
                    sprintf(res_buf, "[CALC] %s = %d\n", expr, result);
                    append_to_output(out_buf, out_size, res_buf);
                    char ibuf[32];
                    itoa(result, ibuf, 10);
                    script_set_var("RESULT", ibuf);
                }
            } else {
                append_to_output(out_buf, out_size, "> Executado: ");
                append_to_output(out_buf, out_size, line);
                append_to_output(out_buf, out_size, "\n");
            }
        } else {
            if (li < sizeof(line_buf) - 1) {
                line_buf[li++] = c;
            }
        }
    }

    if (li > 0) {
        line_buf[li] = '\0';
        char expanded[512];
        script_expand_vars(line_buf, expanded, sizeof(expanded));
        char* line = expanded;
        while (*line == ' ' || *line == '\t') line++;
        if (*line != '#' && *line != ';' && *line != '\0') {
            if (strncmp(line, "echo", 4) == 0) {
                const char* msg = line + 4;
                while (*msg == ' ') msg++;
                append_to_output(out_buf, out_size, msg);
                append_to_output(out_buf, out_size, "\n");
            } else if (strncmp(line, "calc", 4) == 0) {
                const char* expr = line + 5;
                while (*expr == ' ') expr++;
                int result = 0;
                if (script_eval_expr(expr, &result) == 0) {
                    char res_buf[64];
                    sprintf(res_buf, "[CALC] %s = %d\n", expr, result);
                    append_to_output(out_buf, out_size, res_buf);
                }
            }
        }
    }

    return 0;
}

