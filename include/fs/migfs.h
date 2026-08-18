#ifndef FS_MIGFS_H
#define FS_MIGFS_H

#include <libc/stdint.h>
#include <libc/string.h>

#define MIGFS_MAX_FILENAME   32
#define MIGFS_MAX_FILES      64
#define MIGFS_FILE_READONLY  0x01
#define MIGFS_FILE_SYSTEM    0x02

// Estrutura de metadados e conteudo de um arquivo no RAMDisk
typedef struct migfs_file {
    char name[MIGFS_MAX_FILENAME];
    uint32_t size;
    char* data;
    uint32_t flags;
    int in_use;
} migfs_file_t;

// Inicializacao do sistema de arquivos e carga dos arquivos embutidos
void migfs_init(void);

// Operacoes fundamentais de arquivos
int           migfs_create(const char* name, const char* content, size_t size, uint32_t flags);
migfs_file_t* migfs_open(const char* name);
const char*   migfs_read(const char* name);
int           migfs_write(const char* name, const char* content, size_t size);
int           migfs_append(const char* name, const char* content, size_t size);
int           migfs_delete(const char* name);
int           migfs_exists(const char* name);

// Funcoes de listagem e estatisticas
size_t        migfs_get_file_count(void);
size_t        migfs_get_total_used_bytes(void);
migfs_file_t* migfs_get_file_by_index(size_t index);

#endif // FS_MIGFS_H
