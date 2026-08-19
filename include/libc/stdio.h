#ifndef LIBC_STDIO_H
#define LIBC_STDIO_H

#include <libc/stdint.h>
#include <libc/string.h>
#include <fs/migfs.h>
#include <stdarg.h>

#define EOF         (-1)
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

typedef struct FILE {
    migfs_file_t* mig_file;     // Ponteiro para o arquivo no MIGFS (RAMDisk)
    uint32_t pos;               // Posicao atual do cursor de leitura/escrita
    int eof;                    // Flag de fim de arquivo
    int error;                  // Flag de erro
    char mode[8];               // Modo de abertura ("r", "rb", "w", "wb", "a", etc.)
} FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

// Primitivas de manipulacao de arquivos (compatibilidade com DOOM / stdio)
FILE*  fopen(const char* filename, const char* mode);
int    fclose(FILE* stream);
size_t fread(void* ptr, size_t size, size_t count, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream);
int    fseek(FILE* stream, long offset, int whence);
long   ftell(FILE* stream);
int    feof(FILE* stream);
int    ferror(FILE* stream);
int    fflush(FILE* stream);
void   rewind(FILE* stream);
int    remove(const char* filename);

// Funcoes de saida formatada
int    printf(const char* format, ...);
int    sprintf(char* str, const char* format, ...);
int    snprintf(char* str, size_t size, const char* format, ...);
int    vsprintf(char* str, const char* format, va_list args);
int    vsnprintf(char* str, size_t size, const char* format, va_list args);
int    puts(const char* str);
int    putchar(int c);
int    sscanf(const char* str, const char* format, ...);

#endif // LIBC_STDIO_H
