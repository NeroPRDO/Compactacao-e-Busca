/* lzw.c — LZW por blocos (12-bit) + Etapas 1/3/4, com fix do caso (cur==next) */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#ifdef _WIN32
  #include <io.h>
  #ifndef fileno
    #define fileno _fileno
  #endif
#else
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <fcntl.h>
#endif
#include "lzw.h"
#include "index.h"
#include "meuprog.h"

#define LZW_BITS 12
#define LZW_MAX_CODES (1<<LZW_BITS)
#define LZW_FIRST_CODE 256

static Ed2cStats g_last_stats = {0};
void ed2c_get_last_stats(Ed2cStats *st){ if(st) *st = g_last_stats; }

/* --- encoder dict --- */
typedef struct {
    int16_t prefix[LZW_MAX_CODES];
    uint8_t suffix[LZW_MAX_CODES];
    int32_t h_key[8192];
    int16_t h_code[8192];
} LzwDict;

static void lzw_dict_init(LzwDict *d){
    for(int i=0;i<LZW_MAX_CODES;i++){ d->prefix[i]=-1; d->suffix[i]=(uint8_t)i; }
    for(int i=0;i<8192;i++){ d->h_key[i]=-1; d->h_code[i]=-1; }
}
static inline int lzw_hash(int p,int c){ uint32_t x=((uint32_t)p<<5)^(uint32_t)c^((uint32_t)p>>3); return (int)(x&(8192-1)); }
static int lzw_find(const LzwDict *d,int p,int c){
    int key=(p<<8)|c; int idx=lzw_hash(p,c);
    for(int i=0;i<8192;i++){
        int j=(idx+i)&(8192-1);
        if(d->h_key[j]==key) return d->h_code[j];
        if(d->h_key[j]==-1) break;
    }
    return -1;
}
static int lzw_insert(LzwDict *d,int p,int c,int code){
    int key=(p<<8)|c; int idx=lzw_hash(p,c);
    for(int i=0;i<8192;i++){
        int j=(idx+i)&(8192-1);
        if(d->h_key[j]==-1){
            d->h_key[j]=key; d->h_code[j]=(int16_t)code;
            d->prefix[code]=(int16_t)p; d->suffix[code]=(uint8_t)c;
            return 0;
        }
    }
    return -1;
}

/* --- bit writer 12 --- */
typedef struct { uint8_t *data; size_t size,cap; uint32_t bitbuf; int bitcount; } BitW;
static void bw_init(BitW *bw){ bw->data=NULL; bw->size=0; bw->cap=0; bw->bitbuf=0; bw->bitcount=0; }
static void bw_free(BitW *bw){ free(bw->data); }
static int bw_reserve(BitW *bw,size_t need){
    if(bw->size+need<=bw->cap) return 0;
    size_t n=bw->cap?bw->cap*2:256;
    while(bw->size+need>n) n*=2;
    uint8_t *p=(uint8_t*)realloc(bw->data,n); if(!p) return -1;
    bw->data=p; bw->cap=n; return 0;
}
static int bw_put_byte(BitW *bw,uint8_t b){ if(bw_reserve(bw,1)!=0) return -1; bw->data[bw->size++]=b; return 0; }
static int bw_put_code12(BitW *bw,uint16_t code){
    bw->bitbuf|=((uint32_t)code)<<bw->bitcount; bw->bitcount+=12;
    while(bw->bitcount>=8){
        if(bw_put_byte(bw,(uint8_t)(bw->bitbuf&0xFF))!=0) return -1;
        bw->bitbuf>>=8; bw->bitcount-=8;
    }
    return 0;
}
static int bw_flush(BitW *bw){
    if(bw->bitcount>0){
        if(bw_put_byte(bw,(uint8_t)(bw->bitbuf&0xFF))!=0) return -1;
        bw->bitbuf=0; bw->bitcount=0;
    }
    return 0;
}

int lzw_compress_block(const uint8_t *in,size_t in_len,uint8_t **out,size_t *out_len){
    if(in_len==0){ *out=NULL; *out_len=0; return 0; }
    LzwDict dict; lzw_dict_init(&dict);
    int next=LZW_FIRST_CODE; int max=LZW_MAX_CODES-1;
    BitW bw; bw_init(&bw);
    int w=in[0];
    for(size_t i=1;i<in_len;++i){
        int k=in[i]&0xFF; int f=lzw_find(&dict,w,k);
        if(f!=-1){
            w=f;
        } else {
            if(bw_put_code12(&bw,(uint16_t)w)!=0){ bw_free(&bw); return -1; }
            if(next<=max){
                if(lzw_insert(&dict,w,k,next)!=0){ bw_free(&bw); return -1; }
                next++;
            }
            w=k;
        }
    }
    if(bw_put_code12(&bw,(uint16_t)w)!=0){ bw_free(&bw); return -1; }
    if(bw_flush(&bw)!=0){ bw_free(&bw); return -1; }
    *out=bw.data; *out_len=bw.size; return 0;
}

/* --- bit reader 12 --- */
typedef struct { const uint8_t *data; size_t size,pos; uint32_t bitbuf; int bitcount; } BitR;
static void br_init(BitR *br,const uint8_t *d,size_t n){ br->data=d; br->size=n; br->pos=0; br->bitbuf=0; br->bitcount=0; }
static int br_get_code12(BitR *br,uint16_t *out){
    while(br->bitcount>=0 && br->bitcount<12){
        if(br->pos>=br->size) return 0;
        br->bitbuf|=((uint32_t)br->data[br->pos++])<<br->bitcount;
        br->bitcount+=8;
    }
    *out=(uint16_t)(br->bitbuf&0x0FFF);
    br->bitbuf>>=12; br->bitcount-=12;
    return 1;
}

/* --- decoder com fix --- */
int lzw_decompress_block(const uint8_t *in,size_t in_len,uint8_t *out,size_t out_len){
    if(out_len==0) return 0;
    int16_t prefix[LZW_MAX_CODES]; uint8_t suffix[LZW_MAX_CODES]; uint8_t stack[LZW_MAX_CODES];
    for(int i=0;i<256;i++){ prefix[i]=-1; suffix[i]=(uint8_t)i; }
    for(int i=256;i<LZW_MAX_CODES;i++){ prefix[i]=-1; suffix[i]=0; }
    int next=LZW_FIRST_CODE; int max=LZW_MAX_CODES-1;
    BitR br; br_init(&br,in,in_len);
    uint16_t code; int r=br_get_code12(&br,&code); if(r!=1) return -1; if(code>=256) return -1;
    size_t pos=0; out[pos++]=(uint8_t)code; uint16_t prev=code;
    while(pos<out_len){
        r=br_get_code12(&br,&code); if(r!=1) return -1;
        int cur=code; int top=0;
        if(cur<next){
            int c=cur; while(c>=256){ stack[top++]=suffix[c]; c=prefix[c]; } stack[top++]=(uint8_t)c;
        } else if(cur==next){
            int c=prev; while(c>=256){ stack[top++]=suffix[c]; c=prefix[c]; } uint8_t fc=(uint8_t)c; stack[top++]=fc;
        } else {
            return -1;
        }
        for(int i=top-1;i>=0 && pos<out_len; --i){ out[pos++]=stack[i]; }
        if(next<=max){
            int t = (cur<next? cur: prev); while(t>=256) t=prefix[t]; uint8_t fc=(uint8_t)t;
            prefix[next]=prev; suffix[next]=fc; next++;
        }
        prev=cur;
    }
    return 0;
}

/* --- fd helpers --- */
static int fd_full_write(int fd, const void *buf, size_t n){
    const unsigned char *p=(const unsigned char*)buf; size_t left=n;
    while(left>0){
#ifdef _WIN32
        int w=_write(fd,p,(unsigned int)left);
#else
        ssize_t w=write(fd,p,left);
#endif
        if(w<=0){ return -1; }
        p+=w; left-=(size_t)w;
    }
    return 0;
}
static int fd_full_read(int fd, void *buf, size_t n){
    unsigned char *p=(unsigned char*)buf; size_t left=n;
    while(left>0){
#ifdef _WIN32
        int r=_read(fd,p,(unsigned int)left);
#else
        ssize_t r=read(fd,p,left);
#endif
        if(r<=0){ return -1; }
        p+=r; left-=(size_t)r;
    }
    return 0;
}

/* --- Comandos --- */
extern int   g_print_offsets; extern FILE *g_offsets_stream;

int cmd_compactar(const char *in_path,const char *out_path,uint32_t blk_orig_len){
    g_last_stats=(Ed2cStats){0};
    FILE *fin=fopen(in_path,"rb"); if(!fin){ perror("fopen in"); return 1; }
    FILE *fout=fopen(out_path,"wb+"); if(!fout){ perror("fopen out"); fclose(fin); return 2; }
    int fd = fileno(fout);

    Ed2cHeader h; memset(&h,0,sizeof(h));
    memcpy(h.magic,ED2C_MAGIC,4); h.version=ED2C_VERSION; h.blk_orig_len=blk_orig_len; h.algo=ED2C_ALGO_LZW; h.flags=ED2C_FLAG_RESET_PER_BLOCK;
    if (ed2c_header_write(fd,&h)!=0){ fclose(fin); fclose(fout); return 3; }

    uint8_t *inbuf=(uint8_t*)malloc(blk_orig_len); if(!inbuf){ fclose(fin); fclose(fout); return 4; }
    Ed2cIndexEntry *vec=NULL; size_t vcap=0, vsize=0;
    uint64_t orig_off=0, sum_orig=0, sum_comp=0;

    for(;;){
        size_t n=fread(inbuf,1,blk_orig_len,fin);
        if(n==0){
            if(ferror(fin)){ perror("fread"); free(inbuf); fclose(fin); fclose(fout); return 5;}
            break;
        }
        uint8_t *cbuf=NULL; size_t clen=0;
        if(lzw_compress_block(inbuf,n,&cbuf,&clen)!=0){ free(inbuf); fclose(fin); fclose(fout); return 6; }

#ifdef _WIN32
        long long comp_off=_lseeki64(fileno(fout),0,SEEK_CUR);
#else
        off_t comp_off=lseek(fileno(fout),0,SEEK_CUR);
#endif
        if(comp_off<0){ free(cbuf); free(inbuf); fclose(fin); fclose(fout); return 7; }

        if(clen>0){
            if(fd_full_write(fd,cbuf,clen)!=0){ perror("write"); free(cbuf); free(inbuf); fclose(fin); fclose(fout); return 8; }
        }
        free(cbuf);

        if(vsize==vcap){
            size_t ncap=vcap?vcap*2:128;
            void *p=realloc(vec,ncap*sizeof(Ed2cIndexEntry));
            if(!p){ free(vec); free(inbuf); fclose(fin); fclose(fout); return 4; }
            vec=p; vcap=ncap;
        }
        vec[vsize]=(Ed2cIndexEntry){ .orig_off=orig_off, .orig_len=(uint32_t)n, .comp_off=(uint64_t)comp_off, .comp_len=(uint32_t)clen, .flags=0 };
        vsize++;
        orig_off += (uint64_t)n; sum_orig += (uint64_t)n; sum_comp += (uint64_t)clen;
        if(n<blk_orig_len) break;
    }

    free(inbuf); fclose(fin);
#ifdef _WIN32
    long long idx_off=_lseeki64(fd,0,SEEK_CUR);
#else
    off_t idx_off=lseek(fd,0,SEEK_CUR);
#endif
    if(idx_off<0){ free(vec); fclose(fout); return 7; }
    if(ed2c_index_write(fd,vec,(uint64_t)vsize)!=0){ free(vec); fclose(fout); return 8; }

    h.index_offset=(uint64_t)idx_off; h.index_length=(uint64_t)(vsize*sizeof(Ed2cIndexEntry));
    if(ed2c_header_write(fd,&h)!=0){ free(vec); fclose(fout); return 8; }

    g_last_stats.blocks=(uint64_t)vsize; g_last_stats.orig_bytes=sum_orig; g_last_stats.comp_bytes=sum_comp;
    free(vec); fclose(fout); return 0;
}

int cmd_buscar_compactado(const char *comp_path,const uint8_t *pattern,size_t m,uint64_t *out_count){
    if(out_count) *out_count=0;
    if(m==0){ fprintf(stderr,"Erro: padrao de tamanho zero.\n"); return 2; }

    FILE *f=fopen(comp_path,"rb"); if(!f){ perror("fopen comp"); return 1; } int fd=fileno(f);
    Ed2cHeader h; if(ed2c_header_read(fd,&h)!=0){ fclose(f); fprintf(stderr,"Arquivo .ed2c invalido.\n"); return 3; }
    if(h.algo!=ED2C_ALGO_LZW){ fclose(f); fprintf(stderr,"Algoritmo nao suportado.\n"); return 3; }

    Ed2cIndexEntry *entries=NULL; uint64_t count=0;
    if(ed2c_index_read(fd,&h,&entries,&count)!=0){ fclose(f); fprintf(stderr,"Falha ao ler indice.\n"); return 5; }

    uint64_t sum_orig=0, sum_comp=0;
    for(uint64_t i=0;i<count;++i){ sum_orig+=entries[i].orig_len; sum_comp+=entries[i].comp_len; }
    g_last_stats=(Ed2cStats){ .blocks=count, .orig_bytes=sum_orig, .comp_bytes=sum_comp };

    uint8_t *cbuf=NULL; size_t ccap=0; uint8_t *obuf=NULL; size_t obcap=0;
    size_t *pi=(size_t*)malloc(m*sizeof(size_t)); if(!pi){ free(entries); fclose(f); return 4; }
    size_t q=0; pi[0]=0; for(size_t i=1;i<m;++i){ while(q>0 && pattern[q]!=pattern[i]) q=pi[q-1]; if(pattern[q]==pattern[i]) ++q; pi[i]=q; } q=0;

    uint64_t total=0;
    for(uint64_t i=0;i<count;++i){
        const Ed2cIndexEntry *e=&entries[i];
        if(e->comp_len>ccap){ uint8_t *nc=(uint8_t*)realloc(cbuf,e->comp_len); if(!nc){ free(cbuf); free(obuf); free(entries); free(pi); fclose(f); return 4; } cbuf=nc; ccap=e->comp_len; }
        if(e->orig_len>obcap){ uint8_t *no=(uint8_t*)realloc(obuf,e->orig_len); if(!no){ free(cbuf); free(obuf); free(entries); free(pi); fclose(f); return 4; } obuf=no; obcap=e->orig_len; }

#ifdef _WIN32
        if(_lseeki64(fd,(long long)e->comp_off,SEEK_SET)<0){ free(cbuf); free(obuf); free(entries); free(pi); fclose(f); return 7; }
#else
        if(lseek(fd,(off_t)e->comp_off,SEEK_SET)<0){ free(cbuf); free(obuf); free(entries); free(pi); fclose(f); return 7; }
#endif
        if(fd_full_read(fd,cbuf,e->comp_len)!=0){ free(cbuf); free(obuf); free(entries); free(pi); fclose(f); return 10; }
        if(lzw_decompress_block(cbuf,e->comp_len,obuf,e->orig_len)!=0){
            fprintf(stderr,"Erro LZW decompress (bloco %llu).\n",(unsigned long long)i);
            free(cbuf); free(obuf); free(entries); free(pi); fclose(f); return 9;
        }
        for(uint32_t j=0;j<e->orig_len;++j){
            uint8_t b=obuf[j];
            while(q>0 && pattern[q]!=b) q=pi[q-1];
            if(pattern[q]==b){
                ++q;
                if(q==m){
                    uint64_t end=e->orig_off+j; uint64_t start=end+1-(uint64_t)m;
                    if(g_print_offsets){ FILE *dst = g_offsets_stream ? g_offsets_stream : stdout; fprintf(dst, "%" PRIu64 "\n", start); }
                    total++; q=pi[q-1];
                }
            }
        }
    }
    if(out_count) *out_count=total;
    free(cbuf); free(obuf); free(entries); free(pi); fclose(f); return 0;
}

int cmd_descompactar(const char *comp_path, const char *out_path){
    g_last_stats=(Ed2cStats){0};
    FILE *f=fopen(comp_path,"rb"); if(!f){ perror("fopen comp"); return 1; } int fd=fileno(f);
    Ed2cHeader h; if(ed2c_header_read(fd,&h)!=0){ fclose(f); fprintf(stderr,"Arquivo .ed2c invalido.\n"); return 3; }
    Ed2cIndexEntry *entries=NULL; uint64_t count=0; if(ed2c_index_read(fd,&h,&entries,&count)!=0){ fclose(f); fprintf(stderr,"Falha ao ler indice.\n"); return 5; }

    uint64_t sum_orig=0, sum_comp=0; for(uint64_t i=0;i<count;++i){ sum_orig+=entries[i].orig_len; sum_comp+=entries[i].comp_len; }
    g_last_stats=(Ed2cStats){ .blocks=count, .orig_bytes=sum_orig, .comp_bytes=sum_comp };

    FILE *g=fopen(out_path,"wb"); if(!g){ perror("fopen out"); free(entries); fclose(f); return 2; }
    uint8_t *cbuf=NULL; size_t ccap=0; uint8_t *obuf=NULL; size_t obcap=0;

    for(uint64_t i=0;i<count;++i){
        const Ed2cIndexEntry *e=&entries[i];
        if(e->comp_len>ccap){ uint8_t *nc=(uint8_t*)realloc(cbuf,e->comp_len); if(!nc){ free(cbuf); free(obuf); free(entries); fclose(g); fclose(f); return 4; } cbuf=nc; ccap=e->comp_len; }
        if(e->orig_len>obcap){ uint8_t *no=(uint8_t*)realloc(obuf,e->orig_len); if(!no){ free(cbuf); free(obuf); free(entries); fclose(g); fclose(f); return 4; } obuf=no; obcap=e->orig_len; }

#ifdef _WIN32
        if(_lseeki64(fd,(long long)e->comp_off,SEEK_SET)<0){ free(cbuf); free(obuf); free(entries); fclose(g); fclose(f); return 7; }
#else
        if(lseek(fd,(off_t)e->comp_off,SEEK_SET)<0){ free(cbuf); free(obuf); free(entries); fclose(g); fclose(f); return 7; }
#endif
        if(fd_full_read(fd,cbuf,e->comp_len)!=0){ free(cbuf); free(obuf); free(entries); fclose(g); fclose(f); return 10; }
        if(lzw_decompress_block(cbuf,e->comp_len,obuf,e->orig_len)!=0){ free(cbuf); free(obuf); free(entries); fclose(g); fclose(f); return 9; }
        if(fwrite(obuf,1,e->orig_len,g)!=e->orig_len){ perror("fwrite"); free(cbuf); free(obuf); free(entries); fclose(g); fclose(f); return 8; }
    }
    free(cbuf); free(obuf); free(entries); fclose(g); fclose(f); return 0;
}