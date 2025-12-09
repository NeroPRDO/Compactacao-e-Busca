/* Main - CLI + menu interativo e flags de saída */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "meuprog.h"
#include "cli.h"
#include "kmp.h"
#include "lzw.h"
#include "sysmon.h"
#include "types.h"

int   g_print_offsets = 1;
FILE *g_offsets_stream = NULL;

static void print_usage(void){
    printf("Usage:\n");
    printf("  %s compactar <in.txt> <out.ed2c> [blk_mib]\n", MEUPROG_NAME);
    printf("  %s buscar_simples <in.txt> \"<substring>\" [--count-only] [--silent] [--out=FILE]\n", MEUPROG_NAME);
    printf("  %s buscar_compactado <in.ed2c> \"<substring>\" [--count-only] [--silent] [--out=FILE]\n", MEUPROG_NAME);
    printf("  %s descompactar <in.ed2c> <out.txt>\n", MEUPROG_NAME);
}

static void print_report_generic(const char *title, double secs, uint64_t kb, int rc, uint64_t matches){
    printf("\n=== Report: %s ===\n", title);
    printf("Status ..........: %s (rc=%d)\n", rc==0? "OK":"ERROR", rc);
    printf("Time elapsed ....: %.6f s\n", secs);
    if(kb>0) printf("Memory approx ...: %llu KB (Working Set)\n", (unsigned long long)kb);
    else     printf("Memory approx ...: N/A\n");
    if(matches || rc==0) printf("Total matches ...: %llu\n", (unsigned long long)matches);
    Ed2cStats st={0}; ed2c_get_last_stats(&st);
    if(st.blocks){
        printf("Blocks ..........: %llu\n", (unsigned long long)st.blocks);
        printf("Original bytes ..: %llu\n", (unsigned long long)st.orig_bytes);
        printf("Compressed bytes : %llu\n", (unsigned long long)st.comp_bytes);
        if(st.comp_bytes){
            double ratio=(double)st.orig_bytes/(double)st.comp_bytes;
            printf("Compression ratio: %.3fx (orig/comp)\n", ratio);
        }
    }
    printf("=========================\n\n");
}

static void print_report_descompact(double secs, uint64_t kb, int rc){
    printf("\n=== Report: Descompactacao (CLI) ===\n");
    printf("Status ..........: %s (rc=%d)\n", rc==0? "OK":"ERROR", rc);
    printf("Time elapsed ....: %.6f s\n", secs);
    if(kb>0) printf("Memory approx ...: %llu KB (Working Set)\n", (unsigned long long)kb);
    else     printf("Memory approx ...: N/A\n");
    Ed2cStats st={0}; ed2c_get_last_stats(&st);
    if(st.blocks){
        printf("Blocks ..........: %llu\n", (unsigned long long)st.blocks);
        printf("Compressed bytes : %llu\n", (unsigned long long)st.comp_bytes);
        printf("Decompressed bytes: %llu\n", (unsigned long long)st.orig_bytes);
        if(st.comp_bytes){
            double ratio=(double)st.orig_bytes/(double)st.comp_bytes;
            printf("Compression ratio: %.3fx (orig/comp)\n", ratio);
        }
    }
    printf("=========================\n\n");
}

int   g_print_offsets; FILE *g_offsets_stream;

static void parse_output_flags(int argc, char **argv, int start, int *silent){
    *silent = 0;
    for(int i=start;i<argc;++i){
        if(strcmp(argv[i],"--count-only")==0){
            g_print_offsets = 0;
        } else if(strcmp(argv[i],"--silent")==0){
            g_print_offsets = 0; *silent = 1;
        } else if(strncmp(argv[i],"--out=",6)==0){
            const char *path = argv[i]+6;
            if(g_offsets_stream){ fclose(g_offsets_stream); g_offsets_stream=NULL; }
            g_offsets_stream = fopen(path,"wb");
            if(!g_offsets_stream){ perror("fopen --out"); }
        }
    }
}

int main(int argc, char **argv) {
    printf("%s v%s\n", MEUPROG_NAME, MEUPROG_VERSION);
    if (argc <= 1) { return run_ui(); }

    if (strcmp(argv[1], "compactar") == 0) {
        if (argc < 4) { print_usage(); return 2; }
        const char *in = argv[2]; const char *out = argv[3];
        uint32_t blk_mib = (argc >= 5) ? (uint32_t)strtoul(argv[4], NULL, 10) : 4u;
        uint32_t blk = blk_mib * 1024u * 1024u;
        Timer t; timer_start(&t); int rc = cmd_compactar(in, out, blk);
        double secs = timer_elapsed_sec(&t); uint64_t kb = mem_current_kb();
        print_report_generic("Compactacao (CLI)", secs, kb, rc, 0);
        if(g_offsets_stream){ fclose(g_offsets_stream); g_offsets_stream=NULL; }
        return rc;
    }

    if (strcmp(argv[1], "buscar_simples") == 0) {
        if (argc < 4) { print_usage(); return 2; }
        const char *in = argv[2]; const char *pat = argv[3];
        int silent=0; g_print_offsets = 1;
        if(g_offsets_stream){ fclose(g_offsets_stream); g_offsets_stream=NULL; }
        parse_output_flags(argc, argv, 4, &silent);
        if(!silent && g_print_offsets && !g_offsets_stream){ printf("Offsets (0-based, bytes):\n"); }
        uint64_t count=0; Timer t; timer_start(&t);
        int rc = cmd_buscar_simples(in, (const unsigned char*)pat, strlen(pat), DEFAULT_STREAM_BUF, &count);
        double secs = timer_elapsed_sec(&t); uint64_t kb = mem_current_kb();
        print_report_generic("Busca em arquivo original (CLI)", secs, kb, rc, count);
        if(g_offsets_stream){ fclose(g_offsets_stream); g_offsets_stream=NULL; }
        return rc;
    }

    if (strcmp(argv[1], "buscar_compactado") == 0) {
        if (argc < 4) { print_usage(); return 2; }
        const char *in = argv[2]; const char *pat = argv[3];
        int silent=0; g_print_offsets = 1;
        if(g_offsets_stream){ fclose(g_offsets_stream); g_offsets_stream=NULL; }
        parse_output_flags(argc, argv, 4, &silent);
        if(!silent && g_print_offsets && !g_offsets_stream){ printf("Offsets (0-based, bytes):\n"); }
        uint64_t count=0; Timer t; timer_start(&t);
        int rc = cmd_buscar_compactado(in, (const unsigned char*)pat, strlen(pat), &count);
        double secs = timer_elapsed_sec(&t); uint64_t kb = mem_current_kb();
        print_report_generic("Busca em arquivo compactado (CLI)", secs, kb, rc, count);
        if(g_offsets_stream){ fclose(g_offsets_stream); g_offsets_stream=NULL; }
        return rc;
    }

    if (strcmp(argv[1], "descompactar") == 0) {
        if (argc < 4) { print_usage(); return 2; }
        const char *in = argv[2]; const char *out = argv[3];
        Timer t; timer_start(&t); int rc = cmd_descompactar(in, out);
        double secs = timer_elapsed_sec(&t); uint64_t kb = mem_current_kb();
        print_report_descompact(secs, kb, rc);
        if(g_offsets_stream){ fclose(g_offsets_stream); g_offsets_stream=NULL; }
        return rc;
    }

    print_usage();
    if(g_offsets_stream){ fclose(g_offsets_stream); g_offsets_stream=NULL; }
    return 2;
}