#ifndef FS_MIGFS_H
#define FS_MIGFS_H

#include <libc/stdint.h>
#include <libc/string.h>

#define MIGFS_MAX_FILENAME   32
#define MIGFS_MAX_FILES      64
#define MIGFS_FILE_READONLY  0x01
#define MIGFS_FILE_SYSTEM    0x02

#define MIGFS_MAGIC          0x4D494746 // "MIGF" (0x4D494746)
#define MIGFS_VERSION        1
#define MIGFS_SUPER_LBA      1025
#define MIGFS_FILE_TABLE_LBA 1026
#define MIGFS_FILE_TABLE_SECTORS 8      // 64 arquivos * 64 bytes = 4096 bytes = 8 setores
#define MIGFS_DATA_START_LBA 1035

// Estrutura em disco de metadados de arquivo (64 bytes)
typedef struct migfs_disk_entry {
    char name[MIGFS_MAX_FILENAME];
    uint32_t size;
    uint32_t flags;
    uint32_t start_sector;
    uint32_t sector_count;
    uint32_t in_use;
    uint32_t reserved[3];
} __attribute__((packed)) migfs_disk_entry_t;

// Superbloco do MIGFS em disco (512 bytes)
typedef struct migfs_superblock {
    uint32_t magic;
    uint32_t version;
    uint32_t file_count;
    uint32_t next_free_lba;
    uint32_t total_sectors;
    uint32_t flags;
    char label[32];
    uint8_t reserved[456];
} __attribute__((packed)) migfs_superblock_t;

// Estrutura de metadados e conteudo de um arquivo no RAMDisk
typedef struct migfs_file {
    char name[MIGFS_MAX_FILENAME];
    uint32_t size;
    char* data;
    uint32_t flags;
    int in_use;
} migfs_file_t;

// Inicializacao do sistema de arquivos e carga dos arquivos embutidos / do disco
void          migfs_init(void);

// Persistencia em disco ATA
int           migfs_sync_to_disk(void);
int           migfs_load_from_disk(void);
int           migfs_format_disk(void);
int           migfs_is_persisted(void);

// Operacoes fundamentais de arquivos
int           migfs_create(const char* name, const char* content, size_t size, uint32_t flags);
int           migfs_add_buffer(const char* name, char* data, size_t size, uint32_t flags);
migfs_file_t* migfs_open(const char* name);
const char*   migfs_read(const char* name);
int           migfs_write(const char* name, const char* content, size_t size);
int           migfs_append(const char* name, const char* content, size_t size);
int           migfs_delete(const char* name);
int           migfs_exists(const char* name);
int           load_doom_wad_from_disk(void);

// Funcoes de listagem e estatisticas
size_t        migfs_get_file_count(void);
size_t        migfs_get_total_used_bytes(void);
migfs_file_t* migfs_get_file_by_index(size_t index);

#endif // FS_MIGFS_H

