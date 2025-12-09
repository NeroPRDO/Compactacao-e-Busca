/* index.c — leitura/gravação de cabeçalho e índice do .ed2c */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "index.h"

#ifdef _WIN32
  #include <io.h>
  #define lseek64 _lseeki64
#else
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <fcntl.h>
  #define lseek64 lseek
#endif

static int fd_full_write(int fd, const void *buf, size_t n) {
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

static int fd_full_read(int fd, void *buf, size_t n) {
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

int ed2c_header_write(int fd, const Ed2cHeader *h){
    if(lseek64(fd,0,SEEK_SET)<0) return -1;
    if(fd_full_write(fd,h,sizeof(*h))!=0) return -1;
    return 0;
}

int ed2c_header_read (int fd, Ed2cHeader *h){
    if(lseek64(fd,0,SEEK_SET)<0) return -1;
    if(fd_full_read(fd,h,sizeof(*h))!=0) return -1;
    if(memcmp(h->magic,ED2C_MAGIC,4)!=0) return -1;
    if(h->version!=ED2C_VERSION) return -1;
    return 0;
}

int ed2c_index_write(int fd, const Ed2cIndexEntry *entries, uint64_t count){
    size_t bytes = (size_t)(count * (uint64_t)sizeof(Ed2cIndexEntry));
    if(bytes==0) return 0;
    if(fd_full_write(fd,entries,bytes)!=0) return -1;
    return 0;
}

int ed2c_index_read (int fd, const Ed2cHeader *h, Ed2cIndexEntry **out_entries, uint64_t *out_count){
    if(h->index_length==0){ *out_entries=NULL; *out_count=0; return 0; }
    if(lseek64(fd,(long long)h->index_offset,SEEK_SET)<0) return -1;
    uint64_t count = h->index_length / sizeof(Ed2cIndexEntry);
    size_t bytes = (size_t)(count * (uint64_t)sizeof(Ed2cIndexEntry));
    Ed2cIndexEntry *v = (Ed2cIndexEntry*)malloc(bytes);
    if(!v) return -1;
    if(fd_full_read(fd,v,bytes)!=0){ free(v); return -1; }
    *out_entries=v; *out_count=count; return 0;
}