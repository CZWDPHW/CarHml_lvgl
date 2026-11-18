#include "carmw/car_ymodem.h"
#include "carmw/car_md5.h"
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while(0)
typedef struct {
    uint8_t packet[1029], rx[8]; size_t used, need, head, tail;
    const uint8_t *source; size_t size, accepted;
    unsigned packets, retransmits, eots, progress, last_progress;
    bool header, final, retry, reject, no_eot, cancel, io_error;
} peer_t;
static void push(peer_t *p, uint8_t b) { CHECK(p->head<sizeof(p->rx)); p->rx[p->head++]=b; }
static int read_peer(void *user, uint8_t *data, size_t size, int timeout)
{
    peer_t *p=user; (void)timeout; CHECK(size==1);
    if(p->io_error) return -1;
    if(p->tail==p->head) { p->tail=p->head=0; return 0; }
    *data=p->rx[p->tail++];
    if(p->tail==p->head) p->tail=p->head=0;
    return 1;
}
static void receive_packet(peer_t *p)
{
    uint8_t *b=p->packet; size_t payload=p->need-5;
    CHECK((unsigned)b[1]+b[2]==255);
    CHECK(car_ymodem_crc16(b+3,payload)==((unsigned)b[payload+3]<<8|b[payload+4]));
    if(!p->header) {
        CHECK(b[0]==1 && b[1]==0); CHECK(strcmp((char *)b+3,"firmware.bin")==0);
        CHECK(strtoul((char *)b+16,NULL,10)==p->size);
        p->header=true; push(p,6); push(p,0x43);
    } else if(p->accepted==p->size) {
        CHECK(p->eots>0 && !p->no_eot); CHECK(b[1]==0);
        for(size_t i=0;i<128;++i) CHECK(b[3+i]==0);
        p->final=true;
    } else {
        size_t remain=p->size-p->accepted;
        CHECK(payload==(remain>128?1024U:128U));
        CHECK(b[1]==(uint8_t)(p->packets+1));
        size_t take=remain<payload ? remain : payload;
        CHECK(memcmp(b+3,p->source+p->accepted,take)==0);
        for(size_t i=take;i<payload;++i) CHECK(b[3+i]==0x1a);
        if(p->reject || (p->retry && p->retransmits==0)) { ++p->retransmits; push(p,0x15); }
        else { p->accepted+=take; ++p->packets; push(p,0x43); }
    }
    p->used=p->need=0;
}
static int write_peer(void *user, const uint8_t *data, size_t size)
{
    peer_t *p=user;
    if(p->io_error) return -1;
    if(data[0]==0x18 && p->used==0) return (int)size;
    if(size==1 && data[0]==4 && p->used==0) {
        ++p->eots; if(!p->no_eot && p->eots>=2) push(p,6); return 1;
    }
    /* Exercise short writes across headers, data and final packet. */
    size_t take=size>17?17:size;
    if(!p->used) { CHECK(data[0]==1 || data[0]==2); p->need=data[0]==1?133:1029; }
    CHECK(p->used+take<=p->need);
    memcpy(p->packet+p->used,data,take); p->used+=take;
    if(p->used==p->need) receive_packet(p);
    return (int)take;
}
static void progress(void *user, unsigned percent)
{
    peer_t *p=user; CHECK(percent>=p->last_progress && percent<=100);
    CHECK(percent==(unsigned)((uint64_t)p->accepted*100/p->size));
    p->last_progress=percent; ++p->progress;
}
static bool cancelled(void *user) { return ((peer_t *)user)->cancel; }
static int run(size_t size, bool retry, bool reject, bool no_eot, bool bad_md5, bool cancel)
{
    uint8_t *data=malloc(size); CHECK(data);
    for(size_t i=0;i<size;++i) data[i]=(uint8_t)(i*37+i/256);
    char hash[33]; car_md5_t md5; car_md5_init(&md5);
    for(size_t i=0;i<size;++i) car_md5_update(&md5,data+i,1);
    car_md5_final(&md5,hash); if(bad_md5) hash[0]=hash[0]=='a'?'b':'a';
    static unsigned sequence;
    char path[80]; snprintf(path,sizeof(path),"car-ymodem-test-%u.tmp",++sequence);
    FILE *existing=fopen(path,"rb"); CHECK(existing==NULL);
    FILE *file=fopen(path,"wb+"); CHECK(file); CHECK(fwrite(data,1,size,file)==size); rewind(file);
    peer_t peer={0}; peer.source=data; peer.size=size; peer.retry=retry;
    peer.reject=reject; peer.no_eot=no_eot; peer.cancel=cancel; push(&peer,0x43);
    car_port_t port={0}; port.ctx=&peer; port.read=read_peer; port.write=write_peer;
    car_ymodem_t ctx={0}; ctx.port=&port; ctx.user=&peer; ctx.progress=progress;
    ctx.cancelled=cancelled; ctx.retries=3;
    int result=car_ymodem_send(&ctx,file,"firmware.bin",(uint32_t)size,hash);
    if(result==0) {
        CHECK(peer.final && peer.last_progress==100); CHECK(peer.accepted==size);
        CHECK(peer.progress==peer.packets+1); if(retry) CHECK(peer.retransmits==1);
    } else CHECK(!peer.final);
    fclose(file); CHECK(remove(path)==0); free(data); return result;
}
int main(void)
{
    static const char *input[]={"","a","abc","message digest","abcdefghijklmnopqrstuvwxyz",
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"};
    static const char *expected[]={"d41d8cd98f00b204e9800998ecf8427e","0cc175b9c0f1b6a831c399e269772661",
        "900150983cd24fb0d6963f7d28e17f72","f96b697d7cb7938d525a2f31aaf161d0",
        "c3fcd3d76192e4007dfb496cca67e13b","d174ab98d277d9f5a5611c2c9f419d9f"};
    for(unsigned i=0;i<6;++i) {
        car_md5_t md5; char hash[33]; car_md5_init(&md5);
        car_md5_update(&md5,input[i],strlen(input[i])); car_md5_final(&md5,hash);
        if(strcmp(hash,expected[i])!=0) fprintf(stderr,"vector %u: %s expected %s\n",i,hash,expected[i]);
        CHECK(strcmp(hash,expected[i])==0);
    }
    car_md5_t md5; char hash[33]; car_md5_init(&md5);
    for(unsigned i=0;i<1000000;++i) car_md5_update(&md5,"a",1);
    car_md5_final(&md5,hash); CHECK(strcmp(hash,"7707d6ae4e027c70eea2a935c2296f21")==0);
    CHECK(car_ymodem_crc16((const uint8_t *)"123456789",9)==0x31c3);
    CHECK(run(1,false,false,false,false,false)==0);
    CHECK(run(128,false,false,false,false,false)==0);
    CHECK(run(129,true,false,false,false,false)==0);
    CHECK(run(1152,true,false,false,false,false)==0);
    CHECK(run(1153,false,false,false,false,false)==0);
    CHECK(run(270000,false,false,false,false,false)==0); /* block number wraps */
    CHECK(run(128,false,true,false,false,false)==CAR_YMODEM_TIMEOUT);
    CHECK(run(128,false,false,true,false,false)==CAR_YMODEM_TIMEOUT);
    CHECK(run(128,false,false,false,true,false)==CAR_YMODEM_MD5);
    CHECK(run(128,false,false,false,false,true)==CAR_YMODEM_CANCELLED);
    puts("MD5 and Qt YMODEM protocol tests passed"); return 0;
}
