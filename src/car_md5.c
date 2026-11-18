#include "carmw/car_md5.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static uint32_t rotate(uint32_t x, unsigned n) { return (x << n) | (x >> (32U - n)); }
static void transform(car_md5_t *ctx, const uint8_t block[64])
{
    static const uint32_t k[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
    };
    static const unsigned shifts[64] = {
        7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
        5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
        4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
        6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
    };
    uint32_t m[16], a=ctx->state[0], b=ctx->state[1], c=ctx->state[2], d=ctx->state[3];
    for(unsigned i=0; i<16; ++i)
        m[i]=(uint32_t)block[i*4] | (uint32_t)block[i*4+1]<<8 |
             (uint32_t)block[i*4+2]<<16 | (uint32_t)block[i*4+3]<<24;
    for(unsigned i=0; i<64; ++i) {
        uint32_t f; unsigned g;
        if(i<16) { f=(b&c)|(~b&d); g=i; }
        else if(i<32) { f=(d&b)|(~d&c); g=(5*i+1)%16; }
        else if(i<48) { f=b^c^d; g=(3*i+5)%16; }
        else { f=c^(b|~d); g=(7*i)%16; }
        uint32_t next=d;
        d=c; c=b; b+=rotate(a+f+k[i]+m[g], shifts[i]); a=next;
    }
    ctx->state[0]+=a; ctx->state[1]+=b; ctx->state[2]+=c; ctx->state[3]+=d;
}
void car_md5_init(car_md5_t *ctx)
{
    memset(ctx,0,sizeof(*ctx));
    ctx->state[0]=0x67452301; ctx->state[1]=0xefcdab89;
    ctx->state[2]=0x98badcfe; ctx->state[3]=0x10325476;
}
void car_md5_update(car_md5_t *ctx, const void *data, size_t size)
{
    const uint8_t *p=data;
    size_t used=(size_t)(ctx->bytes%64);
    ctx->bytes+=size;
    while(size) {
        size_t take=64-used; if(take>size) take=size;
        memcpy(ctx->block+used,p,take); p+=take; size-=take; used+=take;
        if(used==64) { transform(ctx,ctx->block); used=0; }
    }
}
void car_md5_final(car_md5_t *ctx, char hex[33])
{
    uint64_t bits=ctx->bytes*8;
    uint8_t pad[72]={0x80};
    size_t n=(size_t)(ctx->bytes%64); n=n<56 ? 56-n : 120-n;
    car_md5_update(ctx,pad,n);
    for(unsigned i=0;i<8;++i) pad[i]=(uint8_t)(bits>>(8*i));
    car_md5_update(ctx,pad,8);
    static const char digits[]="0123456789abcdef";
    for(unsigned i=0;i<16;++i) {
        uint8_t v=(uint8_t)(ctx->state[i/4]>>(8*(i%4)));
        hex[2*i]=digits[v>>4]; hex[2*i+1]=digits[v&15];
    }
    hex[32]=0;
}
bool car_md5_file(const char *path, char hex[33])
{
    FILE *f=fopen(path,"rb"); if(!f) return false;
    car_md5_t ctx; car_md5_init(&ctx);
    uint8_t block[8192]; size_t n;
    while((n=fread(block,1,sizeof(block),f))!=0) car_md5_update(&ctx,block,n);
    bool ok=!ferror(f); if(fclose(f)!=0) ok=false;
    if(ok) car_md5_final(&ctx,hex);
    return ok;
}
bool car_md5_read_sidecar(const char *path, char hex[33])
{
    FILE *f=fopen(path,"rb"); if(!f) return false;
    bool ok=true;
    for(unsigned i=0;i<32;++i) {
        int c=fgetc(f);
        if(c==EOF || !isxdigit((unsigned char)c)) { ok=false; break; }
        hex[i]=(char)tolower((unsigned char)c);
    }
    int c=fgetc(f);
    if(c!=EOF && !isspace((unsigned char)c)) ok=false;
    if(ferror(f)) ok=false;
    hex[32]=0; fclose(f); return ok;
}
