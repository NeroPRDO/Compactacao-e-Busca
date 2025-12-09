/* kmp.h — busca de substring em streaming (arquivo original) */
#ifndef KMP_H
#define KMP_H
#include <stdint.h>
#include <stddef.h>
int cmd_buscar_simples(const char *path,
                       const unsigned char *pattern,
                       size_t m,
                       size_t buf_size,
                       uint64_t *out_count);
#endif /* KMP_H */