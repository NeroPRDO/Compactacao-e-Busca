/* index.h — cabeçalho e índice do formato .ed2c */
#ifndef INDEX_H
#define INDEX_H
#include <stdint.h>
#include <stddef.h>
#define ED2C_MAGIC   "ED2C"
#define ED2C_VERSION 1u
#define ED2C_ALGO_LZW  1u
#define ED2C_FLAG_RESET_PER_BLOCK  0x00000001u
#pragma pack(push,1)
typedef struct {
    char     magic[4];
    uint32_t version;
    uint32_t algo;
    uint32_t flags;
    uint32_t blk_orig_len;
    uint64_t index_offset;
    uint64_t index_length;
    uint8_t  reserved[32];
} Ed2cHeader;
typedef struct {
    uint64_t orig_off;
    uint32_t orig_len;
    uint64_t comp_off;
    uint32_t comp_len;
    uint32_t flags;
} Ed2cIndexEntry;
#pragma pack(pop)
int ed2c_header_write(int fd, const Ed2cHeader *h);
int ed2c_header_read (int fd, Ed2cHeader *h);
int ed2c_index_write  (int fd, const Ed2cIndexEntry *entries, uint64_t count);
int ed2c_index_read   (int fd, const Ed2cHeader *h, Ed2cIndexEntry **out_entries, uint64_t *out_count);
#endif /* INDEX_H */