#include <libc/stdio.h>
#include <libc/stdlib.h>
#include <libc/string.h>
#include <kernel/kheap.h>
#include <drivers/vga.h>

#define MAX_OPEN_FILES 32

static FILE file_pool[MAX_OPEN_FILES];
static FILE dummy_stdin  = { NULL, 0, 0, 0, "r" };
static FILE dummy_stdout = { NULL, 0, 0, 0, "w" };
static FILE dummy_stderr = { NULL, 0, 0, 0, "w" };

FILE* stdin  = &dummy_stdin;
FILE* stdout = &dummy_stdout;
FILE* stderr = &dummy_stderr;

FILE* fopen(const char* filename, const char* mode) {
    if (!filename || !mode) return NULL;

    // Normaliza nomes de arquivo (e.g. "./DOOM1.WAD" -> "doom1.wad" ou "DOOM1.WAD")
    const char* p = filename;
    if (p[0] == '.' && (p[1] == '/' || p[1] == '\\')) {
        p += 2;
    }

    migfs_file_t* mf = migfs_open(p);
    if (!mf && (mode[0] == 'w' || mode[0] == 'a')) {
        if (migfs_create(p, "", 0, 0) == 0) {
            mf = migfs_open(p);
        }
    }

    if (!mf) return NULL;

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (file_pool[i].mig_file == NULL) {
            file_pool[i].mig_file = mf;
            file_pool[i].pos = (mode[0] == 'a') ? mf->size : 0;
            file_pool[i].eof = 0;
            file_pool[i].error = 0;
            strncpy(file_pool[i].mode, mode, 7);
            file_pool[i].mode[7] = '\0';
            return &file_pool[i];
        }
    }

    return NULL;
}

int fclose(FILE* stream) {
    if (!stream || stream == stdin || stream == stdout || stream == stderr) {
        return 0;
    }

    stream->mig_file = NULL;
    stream->pos = 0;
    stream->eof = 0;
    stream->error = 0;
    return 0;
}

size_t fread(void* ptr, size_t size, size_t count, FILE* stream) {
    if (!ptr || size == 0 || count == 0 || !stream || !stream->mig_file) {
        return 0;
    }

    migfs_file_t* mf = stream->mig_file;
    size_t total_bytes = size * count;

    if (stream->pos >= mf->size) {
        stream->eof = 1;
        return 0;
    }

    size_t available = mf->size - stream->pos;
    size_t to_read = (total_bytes < available) ? total_bytes : available;

    if (to_read > 0 && mf->data) {
        memcpy(ptr, mf->data + stream->pos, to_read);
        stream->pos += to_read;
    }

    if (stream->pos >= mf->size) {
        stream->eof = 1;
    }

    return to_read / size;
}

size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream) {
    if (!ptr || size == 0 || count == 0 || !stream || !stream->mig_file) {
        return 0;
    }

    migfs_file_t* mf = stream->mig_file;
    size_t total_bytes = size * count;

    if (stream->pos + total_bytes > mf->size) {
        if (migfs_write(mf->name, mf->data, stream->pos + total_bytes) != 0) {
            stream->error = 1;
            return 0;
        }
        mf = migfs_open(mf->name);
        stream->mig_file = mf;
    }

    if (mf && mf->data) {
        memcpy(mf->data + stream->pos, ptr, total_bytes);
        stream->pos += total_bytes;
        return count;
    }

    return 0;
}

int fseek(FILE* stream, long offset, int whence) {
    if (!stream || !stream->mig_file) return -1;

    migfs_file_t* mf = stream->mig_file;
    long new_pos = 0;

    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = (long)stream->pos + offset;
            break;
        case SEEK_END:
            new_pos = (long)mf->size + offset;
            break;
        default:
            return -1;
    }

    if (new_pos < 0) {
        new_pos = 0;
    }
    if (new_pos > (long)mf->size) {
        new_pos = (long)mf->size;
    }

    stream->pos = (uint32_t)new_pos;
    stream->eof = (stream->pos >= mf->size);
    return 0;
}

long ftell(FILE* stream) {
    if (!stream) return -1;
    return (long)stream->pos;
}

int feof(FILE* stream) {
    return stream ? stream->eof : 1;
}

int ferror(FILE* stream) {
    return stream ? stream->error : 1;
}

int fflush(FILE* stream) {
    (void)stream;
    return 0;
}

void rewind(FILE* stream) {
    if (stream) {
        fseek(stream, 0, SEEK_SET);
        stream->eof = 0;
        stream->error = 0;
    }
}

int remove(const char* filename) {
    return migfs_delete(filename);
}

int putchar(int c) {
    vga_putc((char)c);
    return c;
}

int puts(const char* str) {
    if (!str) return EOF;
    vga_puts(str);
    vga_putc('\n');
    return 0;
}

int vsnprintf(char* str, size_t size, const char* format, va_list args) {
    if (!str || size == 0 || !format) return 0;

    size_t written = 0;
    char num_buf[32];

    for (size_t i = 0; format[i] != '\0' && written < size - 1; i++) {
        if (format[i] == '%' && format[i + 1] != '\0') {
            i++;
            int precision = -1;

            // Ignora flags como '-', '+', ' ', '#', '0'
            while (format[i] == '-' || format[i] == '+' || format[i] == ' ' || format[i] == '#' || format[i] == '0') {
                i++;
            }

            // Ignora largura
            while (format[i] >= '0' && format[i] <= '9') {
                i++;
            }

            // Trata precisao (ex: %.4s, %.2f)
            if (format[i] == '.') {
                i++;
                precision = 0;
                while (format[i] >= '0' && format[i] <= '9') {
                    precision = precision * 10 + (format[i] - '0');
                    i++;
                }
            }

            // Ignora modificadores de comprimento (ex: %ld, %hhd, %zu)
            while (format[i] == 'l' || format[i] == 'h' || format[i] == 'z' || format[i] == 'j' || format[i] == 't') {
                i++;
            }

            if (format[i] == 'd' || format[i] == 'i') {
                int val = va_arg(args, int);
                itoa(val, num_buf, 10);
                for (int j = 0; num_buf[j] && written < size - 1; j++) {
                    str[written++] = num_buf[j];
                }
            } else if (format[i] == 'u') {
                unsigned int val = va_arg(args, unsigned int);
                itoa((int)val, num_buf, 10);
                for (int j = 0; num_buf[j] && written < size - 1; j++) {
                    str[written++] = num_buf[j];
                }
            } else if (format[i] == 'x' || format[i] == 'X' || format[i] == 'p') {
                unsigned int val = va_arg(args, unsigned int);
                itoa((int)val, num_buf, 16);
                for (int j = 0; num_buf[j] && written < size - 1; j++) {
                    str[written++] = num_buf[j];
                }
            } else if (format[i] == 's') {
                const char* s = va_arg(args, const char*);
                if (!s) s = "(null)";
                int count = 0;
                while (*s && written < size - 1 && (precision < 0 || count < precision)) {
                    str[written++] = *s++;
                    count++;
                }
            } else if (format[i] == 'c') {
                char c = (char)va_arg(args, int);
                str[written++] = c;
            } else if (format[i] == '%') {
                str[written++] = '%';
            }
        } else {
            str[written++] = format[i];
        }
    }

    str[written] = '\0';
    return (int)written;
}

int snprintf(char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(str, size, format, args);
    va_end(args);
    return ret;
}

int sprintf(char* str, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(str, 4096, format, args);
    va_end(args);
    return ret;
}

int vsprintf(char* str, const char* format, va_list args) {
    return vsnprintf(str, 4096, format, args);
}

int printf(const char* format, ...) {
    char buf[1024];
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    vga_puts(buf);
    return ret;
}

int sscanf(const char* str, const char* format, ...) {
    if (!str || !format) return 0;
    va_list args;
    va_start(args, format);
    int count = 0;

    const char* s = str;
    for (size_t i = 0; format[i] != '\0' && *s != '\0'; i++) {
        if (format[i] == '%') {
            i++;
            if (format[i] == 'd' || format[i] == 'i') {
                while (*s == ' ' || *s == '\t') s++;
                int sign = 1;
                if (*s == '-') { sign = -1; s++; }
                else if (*s == '+') { s++; }
                int val = 0;
                while (*s >= '0' && *s <= '9') {
                    val = val * 10 + (*s - '0');
                    s++;
                }
                int* out = va_arg(args, int*);
                if (out) *out = val * sign;
                count++;
            } else if (format[i] == 's') {
                while (*s == ' ' || *s == '\t') s++;
                char* out = va_arg(args, char*);
                int idx = 0;
                while (*s && *s != ' ' && *s != '\t' && *s != '\n') {
                    if (out) out[idx++] = *s;
                    s++;
                }
                if (out) out[idx] = '\0';
                count++;
            }
        } else if (format[i] == *s) {
            s++;
        }
    }

    va_end(args);
    return count;
}

int fprintf(FILE* stream, const char* format, ...) {
    char buf[1024];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (stream == stdout || stream == stderr || stream == NULL) {
        vga_puts(buf);
    } else {
        fwrite(buf, 1, len, stream);
    }
    return len;
}

int vfprintf(FILE* stream, const char* format, va_list ap) {
    char buf[1024];
    int len = vsnprintf(buf, sizeof(buf), format, ap);
    if (stream == stdout || stream == stderr || stream == NULL) {
        vga_puts(buf);
    } else {
        fwrite(buf, 1, len, stream);
    }
    return len;
}

int rename(const char* oldname, const char* newname) {
    (void)oldname;
    (void)newname;
    return 0;
}

int _vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    return vsnprintf(str, size, format, ap);
}

static int global_errno = 0;
int* __errno(void) {
    return &global_errno;
}

int* (*_imp___errno)(void) = __errno;
int errno = 0;


