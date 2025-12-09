/* kmp.c — KMP streaming */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "kmp.h"

extern int   g_print_offsets;
extern FILE *g_offsets_stream;

static void build_pi(const unsigned char *pat, size_t m, size_t *pi){
    pi[0]=0; size_t k=0;
    for(size_t q=1;q<m;++q){
        while(k>0 && pat[k]!=pat[q]) k=pi[k-1];
        if(pat[k]==pat[q]) ++k;
        pi[q]=k;
    }
}

int cmd_buscar_simples(const char *path,
                       const unsigned char *pattern,
                       size_t m,
                       size_t buf_size,
                       uint64_t *out_count)
{
    if(out_count) *out_count=0;
    if(m==0) return 2;
    FILE *f=fopen(path,"rb");
    if(!f) return 1;

    unsigned char *buf=(unsigned char*)malloc(buf_size);
    if(!buf){ fclose(f); return 4; }
    size_t *pi=(size_t*)malloc(m*sizeof(size_t));
    if(!pi){ free(buf); fclose(f); return 4; }

    build_pi(pattern,m,pi);

    uint64_t pos=0, total=0;
    size_t q=0;
    for(;;){
        size_t n=fread(buf,1,buf_size,f);
        if(n==0){
            if(ferror(f)){ free(pi); free(buf); fclose(f); return 5; }
            break;
        }
        for(size_t i=0;i<n;++i){
            unsigned char b=buf[i];
            while(q>0 && pattern[q]!=b) q=pi[q-1];
            if(pattern[q]==b){
                ++q;
                if(q==m){
                    uint64_t start = pos + i + 1 - (uint64_t)m;
                    if(g_print_offsets){
                        FILE *dst = g_offsets_stream ? g_offsets_stream : stdout;
                        fprintf(dst, "%" PRIu64 "\n", start);
                    }
                    total++;
                    q=pi[q-1];
                }
            }
        }
        pos += n;
    }
    if(out_count) *out_count=total;
    free(pi); free(buf); fclose(f);
    return 0;
}