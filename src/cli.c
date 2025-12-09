/* cli.c — UI interativa (menu) */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "meuprog.h"
#include "kmp.h"
#include "lzw.h"
#include "sysmon.h"
#include "types.h"

static void read_line(char *buf, size_t n){
    if(!fgets(buf,(int)n,stdin)) { buf[0]=0; return; }
    size_t L=strlen(buf);
    if(L && (buf[L-1]=='\n'||buf[L-1]=='\r')) buf[L-1]=0;
}

int run_ui(void){
    for(;;){
        printf("\n=== Menu ===\n");
        printf("1) Compactar arquivo (Etapa 1)\n");
        printf("2) Buscar em arquivo original (Etapa 2)\n");
        printf("3) Buscar em arquivo compactado .ed2c (Etapa 3)\n");
        printf("4) Descompactar .ed2c para .txt (demo)\n");
        printf("0) Sair\n");
        printf("Escolha: ");
        char tmp[1024]; read_line(tmp,sizeof(tmp));
        int op=atoi(tmp);
        if(op==0) return 0;

        if(op==1){
            char in[1024], out[1024], blk[64];
            printf("Caminho do arquivo original: "); read_line(in,sizeof(in));
            printf("Arquivo compactado de saida (.ed2c): "); read_line(out,sizeof(out));
            if(out[0]==0){ printf("Saida invalida.\n"); continue; }
            printf("Tamanho do bloco em MiB (ENTER=4): "); read_line(blk,sizeof(blk));
            unsigned long blk_mib = (blk[0]? strtoul(blk,NULL,10):4ul);
            unsigned long blk_len = blk_mib * 1024ul * 1024ul;
            Timer t; timer_start(&t);
            int rc = cmd_compactar(in,out,(uint32_t)blk_len);
            double secs = timer_elapsed_sec(&t);
            uint64_t kb = mem_current_kb();
            printf("Compactacao concluida: rc=%d\n", rc);
            Ed2cStats st={0}; ed2c_get_last_stats(&st);
            printf("\n=== Report: Compactacao (UI) ===\n");
            printf("Status ..........: %s (rc=%d)\n", rc==0?"OK":"ERROR", rc);
            printf("Time elapsed ....: %.6f s\n", secs);
            printf("Memory approx ...: %llu KB (Working Set)\n", (unsigned long long)kb);
            if(st.blocks){
                printf("Blocks ..........: %llu\n", (unsigned long long)st.blocks);
                printf("Original bytes ..: %llu\n", (unsigned long long)st.orig_bytes);
                printf("Compressed bytes : %llu\n", (unsigned long long)st.comp_bytes);
                if(st.comp_bytes){
                    double ratio=(double)st.orig_bytes/(double)st.comp_bytes;
                    printf("Compression ratio: %.3fx (orig/comp)\n", ratio);
                }
            }
            printf("=========================\n");
        } else if(op==2 || op==3){
            int compactado = (op==3);
            char path[1024], pat[1024];
            printf("Caminho do arquivo %s: ", compactado? ".ed2c":"original"); read_line(path,sizeof(path));
            printf("Substring a buscar: "); read_line(pat,sizeof(pat));
            if(!path[0] || !pat[0]){ printf("Parametros invalidos.\n"); continue; }

            extern int g_print_offsets; extern FILE *g_offsets_stream;
            g_print_offsets = 0;
            if(g_offsets_stream){ fclose(g_offsets_stream); g_offsets_stream=NULL; }

            char yn[16];
            printf("Mostrar offsets? (s/N): "); read_line(yn,sizeof(yn));
            if(yn[0]=='s' || yn[0]=='S'){
                g_print_offsets = 1;
                char outf[1024];
                printf("Salvar offsets em arquivo? (caminho ou ENTER p/ console): "); read_line(outf,sizeof(outf));
                if(outf[0]){
                    g_offsets_stream = fopen(outf,"wb");
                    if(!g_offsets_stream) perror("fopen offsets");
                }else{
                    /* console */
                }
                if(!g_offsets_stream && g_print_offsets){ printf("Offsets (0-based, bytes):\n"); }
            }

            Timer t; timer_start(&t);
            uint64_t count=0; int rc=0;
            if(compactado){
                rc = cmd_buscar_compactado(path,(const unsigned char*)pat,strlen(pat),&count);
            } else {
                rc = cmd_buscar_simples(path,(const unsigned char*)pat,strlen(pat), DEFAULT_STREAM_BUF, &count);
            }
            double secs = timer_elapsed_sec(&t);
            uint64_t kb = mem_current_kb();
            printf("\n=== Report: Busca %s (UI) ===\n", compactado? "compactado":"original");
            printf("Status ..........: %s (rc=%d)\n", rc==0?"OK":"ERROR", rc);
            printf("Time elapsed ....: %.6f s\n", secs);
            printf("Memory approx ...: %llu KB (Working Set)\n", (unsigned long long)kb);
            printf("Total matches ...: %llu\n", (unsigned long long)count);
            printf("=========================\n");
            if(g_offsets_stream){ fclose(g_offsets_stream); g_offsets_stream=NULL; }
        } else if(op==4){
            char in[1024], out[1024];
            printf("Caminho do arquivo .ed2c: "); read_line(in,sizeof(in));
            printf("Arquivo de saida (.txt): "); read_line(out,sizeof(out));
            Timer t; timer_start(&t);
            int rc = cmd_descompactar(in,out);
            double secs = timer_elapsed_sec(&t);
            uint64_t kb = mem_current_kb();
            Ed2cStats st={0}; ed2c_get_last_stats(&st);
            printf("\n=== Report: Descompactacao (UI) ===\n");
            printf("Status ..........: %s (rc=%d)\n", rc==0?"OK":"ERROR", rc);
            printf("Time elapsed ....: %.6f s\n", secs);
            printf("Memory approx ...: %llu KB (Working Set)\n", (unsigned long long)kb);
            if(st.blocks){
                printf("Blocks ..........: %llu\n", (unsigned long long)st.blocks);
                printf("Compressed bytes : %llu\n", (unsigned long long)st.comp_bytes);
                printf("Decompressed bytes: %llu\n", (unsigned long long)st.orig_bytes);
                if(st.comp_bytes){
                    double ratio=(double)st.orig_bytes/(double)st.comp_bytes;
                    printf("Compression ratio: %.3fx (orig/comp)\n", ratio);
                }
            }
            printf("=========================\n");
        } else {
            printf("Opcao invalida.\n");
        }
    }
}