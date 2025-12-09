/* lzw.h — compactação por blocos (LZW 12-bit) + comandos das Etapas 1/3/4 */
#ifndef LZW_H
#define LZW_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
    uint64_t blocks;
    uint64_t orig_bytes;
    uint64_t comp_bytes;
} Ed2cStats;
void ed2c_get_last_stats(Ed2cStats *st);
int lzw_compress_block(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len);
int lzw_decompress_block(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_len);
int cmd_compactar(const char *in_path, const char *out_path, uint32_t blk_orig_len);
int cmd_buscar_compactado(const char *comp_path, const uint8_t *pattern, size_t m, uint64_t *out_count);
int cmd_descompactar(const char *comp_path, const char *out_path);
#endif /* LZW_H */