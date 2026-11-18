#include "carmw/car_ymodem.h"
#include "carmw/car_md5.h"
#include <string.h>

uint16_t car_ymodem_crc16(const uint8_t *data, size_t size)
{
    uint16_t crc=0;
    while(size--) {
        crc^=(uint16_t)*data++<<8;
        for(unsigned i=0;i<8;++i) crc=(uint16_t)((crc<<1)^((crc&0x8000)?0x1021:0));
    }
    return crc;
}
static bool cancelled(car_ymodem_t *c) { return c->cancelled && c->cancelled(c->user); }
static int write_all(car_ymodem_t *c, const uint8_t *p, size_t n)
{
    while(n) {
        if(cancelled(c)) return CAR_YMODEM_CANCELLED;
        int sent=c->port->write(c->port->ctx,p,n);
        if(sent<=0 || (size_t)sent>n) return CAR_YMODEM_IO;
        p+=sent; n-=(size_t)sent;
    }
    return 0;
}
static int wait_byte(car_ymodem_t *c, uint8_t expected, unsigned attempts)
{
    while(attempts--) {
        uint8_t byte;
        if(cancelled(c)) return CAR_YMODEM_CANCELLED;
        int n=c->port->read(c->port->ctx,&byte,1,1000);
        if(n<0) return CAR_YMODEM_IO;
        if(n==1 && byte==0x18) return CAR_YMODEM_CANCELLED;
        if(n==1 && byte==expected) return 0;
    }
    return CAR_YMODEM_TIMEOUT;
}
static void checksum(uint8_t *packet, size_t size)
{
    uint16_t crc=car_ymodem_crc16(packet+3,size);
    packet[size+3]=(uint8_t)(crc>>8); packet[size+4]=(uint8_t)crc;
}
static int send_impl(car_ymodem_t *c, FILE *file, const char *name,
                     uint32_t size, const char *expected)
{
    uint8_t packet[1029]={1,0,255};
    size_t len=strlen(name);
    if(!size || size>0xffffffU || !len || len>110 || strchr(name,'/') || strchr(name,'\\'))
        return CAR_YMODEM_INVALID;
    int result=wait_byte(c,0x43,30); if(result) return result;
    memcpy(packet+3,name,len+1);
    (void)snprintf((char *)packet+4+len,128-len-1,"%u",(unsigned)size);
    checksum(packet,128);
    result=write_all(c,packet,133); if(result) return result;
    result=wait_byte(c,6,30); if(result) return result;
    result=wait_byte(c,0x43,30); if(result) return result;
    uint32_t sent=0; uint8_t number=1;
    car_md5_t md5; car_md5_init(&md5);
    if(c->progress) c->progress(c->user,0);
    while(sent<size) {
        size_t payload=size-sent>128 ? 1024 : 128;
        size_t take=size-sent; if(take>payload) take=payload;
        packet[0]=payload==1024?2:1; packet[1]=number; packet[2]=(uint8_t)~number;
        memset(packet+3,0x1a,payload);
        if(fread(packet+3,1,take,file)!=take) return CAR_YMODEM_IO;
        checksum(packet,payload);
        unsigned retries=c->retries ? c->retries : 4000;
        bool ack=false;
        while(retries--) {
            result=write_all(c,packet,payload+5); if(result) return result;
            uint8_t byte=0;
            int n=c->port->read(c->port->ctx,&byte,1,1000);
            if(n<0) return CAR_YMODEM_IO;
            if(n==1 && byte==0x18) return CAR_YMODEM_CANCELLED;
            if(n==1 && byte==0x43) { ack=true; break; }
        }
        if(!ack) return CAR_YMODEM_TIMEOUT;
        /* Hash and progress count each acknowledged source byte exactly once. */
        car_md5_update(&md5,packet+3,take); sent+=(uint32_t)take; ++number;
        if(c->progress) c->progress(c->user,(unsigned)((uint64_t)sent*100/size));
    }
    char hex[33]; car_md5_final(&md5,hex);
    if(strcmp(hex,expected)!=0) return CAR_YMODEM_MD5;
    bool eot_ack=false;
    for(unsigned i=0;i<30;++i) {
        const uint8_t eot=4;
        result=write_all(c,&eot,1); if(result) return result;
        result=wait_byte(c,6,1);
        if(!result) { eot_ack=true; break; }
        if(result!=CAR_YMODEM_TIMEOUT) return result;
    }
    if(!eot_ack) return CAR_YMODEM_TIMEOUT;
    memset(packet,0,133); packet[0]=1; packet[2]=255; checksum(packet,128);
    /* Qt sends an empty header without waiting for an additional A2 frame. */
    return write_all(c,packet,133);
}
car_ymodem_result_t car_ymodem_send(car_ymodem_t *c, FILE *file,
    const char *name, uint32_t size, const char expected[33])
{
    if(!c || !c->port || !c->port->read || !c->port->write || !file || !name || !expected)
        return CAR_YMODEM_INVALID;
    int result=send_impl(c,file,name,size,expected);
    if(result) { const uint8_t can[2]={0x18,0x18}; (void)c->port->write(c->port->ctx,can,2); }
    return (car_ymodem_result_t)result;
}
