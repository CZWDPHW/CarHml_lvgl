#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "carmw/car_upgrade.h"
#include "carmw/car_port_posix.h"
#include "carmw/car_md5.h"
#include "carmw/car_ymodem.h"
#include "carmw/car_protocol.h"
#include <stdatomic.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

struct car_upgrade {
    car_upgrade_config_t config;
    char usb[CAR_UPGRADE_PATH], home[CAR_UPGRADE_PATH], root[CAR_UPGRADE_PATH], serial[64];
    car_link_t *link;
    pthread_t worker;
    pthread_mutex_t lock;
    _Atomic bool stop;
    car_upgrade_status_t status;
    /* Handshakes are protected by lock; UI alone controls the normal link. */
    int save_request, serial_request;
    bool suspended, restart_thread;
};
static bool cancelled(void *user) { return atomic_load(&((car_upgrade_t *)user)->stop); }
static bool delay(car_upgrade_t *u, unsigned ms)
{
    while(ms && !cancelled(u)) {
        unsigned part=ms>50?50:ms;
        struct timespec ts={0,(long)part*1000000L}; nanosleep(&ts,NULL); ms-=part;
    }
    return !cancelled(u);
}
static bool protocol_delay(void *user, unsigned ms) { return delay(user,ms); }
static void phase(car_upgrade_t *u, unsigned component, car_upgrade_phase_t value)
{
    pthread_mutex_lock(&u->lock); u->status.phase[component]=value;
    ++u->status.revision; pthread_mutex_unlock(&u->lock);
}
static void progress(void *user, unsigned percent)
{
    car_upgrade_t *u=user; pthread_mutex_lock(&u->lock);
    if(u->status.mcu_percent!=percent) { u->status.mcu_percent=percent; ++u->status.revision; }
    pthread_mutex_unlock(&u->lock);
}
static bool join_path(char out[CAR_UPGRADE_PATH], const char *root, const char *name)
{
    int n=snprintf(out,CAR_UPGRADE_PATH,"%s/%s",root,name);
    return n>0 && n<CAR_UPGRADE_PATH;
}
static bool request_save(car_upgrade_t *u)
{
    pthread_mutex_lock(&u->lock); u->save_request=1; pthread_mutex_unlock(&u->lock);
    for(;;) {
        pthread_mutex_lock(&u->lock); int state=u->save_request; pthread_mutex_unlock(&u->lock);
        if(state!=1) return state==2;
        if(!delay(u,20)) return false;
    }
}
static bool serial_acquire(car_upgrade_t *u)
{
    pthread_mutex_lock(&u->lock); u->serial_request=1; pthread_mutex_unlock(&u->lock);
    for(;;) {
        pthread_mutex_lock(&u->lock); int state=u->serial_request; pthread_mutex_unlock(&u->lock);
        if(state!=1) return state==2;
        if(!delay(u,20)) return false;
    }
}
static void serial_release(car_upgrade_t *u)
{
    pthread_mutex_lock(&u->lock); u->serial_request=3; pthread_mutex_unlock(&u->lock);
    for(;;) {
        pthread_mutex_lock(&u->lock); int state=u->serial_request; pthread_mutex_unlock(&u->lock);
        if(state==0 || !delay(u,20)) return;
    }
}
static bool write_all(car_upgrade_t *u, car_port_t *p, const uint8_t *data, size_t count)
{
    while(count && !cancelled(u)) {
        int n=p->write(p->ctx,data,count); if(n<=0 || (size_t)n>count) return false;
        data+=n; count-=(size_t)n;
    }
    return count==0;
}
static bool request_mcu(car_upgrade_t *u, car_port_t *p, const char *name, uint32_t size)
{
    uint8_t payload[128],frame[133]; size_t len=strlen(name);
    if(len>110 || !size || size>0xffffffU) return false;
    payload[0]=(uint8_t)(size>>16); payload[1]=(uint8_t)(size>>8); payload[2]=(uint8_t)size;
    memcpy(payload+3,name,len+1);
    size_t count=car_frame_encode(0xff,2,payload,(uint16_t)(len+4),frame,sizeof(frame));
    /* Qt's 5 second wait plus 4 second send_frame_safe delay. */
    if(!count || !delay(u,9000) || !write_all(u,p,frame,count)) return false;
    uint8_t response[512]; size_t used=0;
    uint64_t started=car_port_now(p);
    while(car_port_now(p)-started<3000 && !cancelled(u)) {
        uint8_t byte; int n=p->read(p->ctx,&byte,1,100);
        if(n<0) return false;
        if(!n) continue;
        if(used==sizeof(response)) { memmove(response,response+1,--used); }
        response[used++]=byte;
        for(size_t start=0;start+6<=used;++start) {
            car_frame_view_t v;
            if(car_frame_inspect(response+start,used-start,&v) && v.checksum_ok &&
               v.cmd_id==0xff && (v.op==CAR_OP_STATUS || v.op==1) && v.len==1 && v.payload[0]==1)
                return delay(u,200);
        }
        /* One byte reads deliberately leave the bootloader's following C unread. */
    }
    return false;
}
static bool transfer_mcu(car_upgrade_t *u, const char *path, const char hash[33])
{
    char cache[CAR_UPGRADE_PATH],flag[CAR_UPGRADE_PATH],local_md5[CAR_UPGRADE_PATH];
    const char *name=strrchr(path,'/'); name=name?name+1:path;
    struct stat st;
    if(strlen(name)>110 || !join_path(cache,u->home,"mcu-upgrade.bin") ||
       !join_path(flag,u->home,"mcu_flag") || !join_path(local_md5,u->home,"bin.md5")) return false;
    phase(u,CAR_UPGRADE_MCU,CAR_UPGRADE_COPYING);
    if(!car_upgrade_copy_file(path,cache,0644)) return false;
    char actual[33];
    if(!car_md5_file(cache,actual) || strcmp(hash,actual) || stat(cache,&st)!=0 ||
       st.st_size<=0 || st.st_size>0xffffff) { remove(cache); return false; }
    /* Keep the flag set after a failed/interrupted transfer so next boot reports it. */
    if(!car_upgrade_atomic_text(flag,"1\n")) { remove(cache); return false; }
    phase(u,CAR_UPGRADE_MCU,CAR_UPGRADE_PREPARING);
    if(!serial_acquire(u)) { remove(cache); return false; }
    car_port_t port={0}; car_port_posix_t storage;
    bool ok=car_port_posix_attach(&port,&storage,u->serial,u->config.baud);
    if(ok) ok=port.open(port.ctx);
    if(ok) ok=request_mcu(u,&port,name,(uint32_t)st.st_size);
    if(ok) {
        phase(u,CAR_UPGRADE_MCU,CAR_UPGRADE_TRANSFERRING);
        FILE *file=fopen(cache,"rb");
        if(!file) ok=false;
        else {
            car_ymodem_t ctx={0}; ctx.port=&port; ctx.user=u;
            ctx.cancelled=cancelled; ctx.progress=progress;
            ctx.wait_ms=protocol_delay;
            ok=car_ymodem_send(&ctx,file,name,(uint32_t)st.st_size,hash)==CAR_YMODEM_OK;
            fclose(file);
        }
    }
    if(port.close) port.close(port.ctx);
    serial_release(u);
    if(ok) ok=car_upgrade_atomic_text(local_md5,hash) && car_upgrade_atomic_text(flag,"0\n");
    remove(cache); return ok;
}
static bool mcu_candidates(car_upgrade_t *u, const char *volume)
{
    DIR *dir=opendir(volume); if(!dir) return false;
    struct dirent *entry; char selected[CAR_UPGRADE_PATH]="",previous[CAR_UPGRADE_PATH]="";
    char local[CAR_UPGRADE_PATH]; bool current=false;
    if(!join_path(local,u->home,"bin.md5")) { closedir(dir); return false; }
    /* Qt sorts filenames and tries later firmware when verification fails. */
    for(;;) {
        rewinddir(dir); selected[0]=0;
        while((entry=readdir(dir))) {
            size_t n=strlen(entry->d_name); if(n<4 || strchr(entry->d_name,'\\')) continue;
            const char *end=entry->d_name+n-4;
            if(strcmp(end,".bin") && strcmp(end,".BIN")) continue;
            char path[CAR_UPGRADE_PATH]; if(!join_path(path,volume,entry->d_name)) continue;
            if(strcmp(path,previous)>0 && (!selected[0] || strcmp(path,selected)<0)) strcpy(selected,path);
        }
        if(!selected[0] || cancelled(u)) break;
        strcpy(previous,selected); char hash[33];
        phase(u,CAR_UPGRADE_MCU,CAR_UPGRADE_CHECKING);
        int result=car_upgrade_verify(selected,local,hash);
        if(result==0) { current=true; continue; }
        if(result<0) continue;
        bool ok=transfer_mcu(u,selected,hash);
        phase(u,CAR_UPGRADE_MCU,ok?CAR_UPGRADE_SUCCESS:CAR_UPGRADE_FAILED);
        closedir(dir); return ok;
    }
    closedir(dir); phase(u,CAR_UPGRADE_MCU,current?CAR_UPGRADE_CURRENT:CAR_UPGRADE_FAILED);
    return current;
}
static void run_package(car_upgrade_t *u, const car_upgrade_package_t *package)
{
    bool success=true,reboot=false;
    pthread_mutex_lock(&u->lock);
    u->status.visible=true; u->status.busy=true; u->status.finished=false;
    u->status.mcu_percent=0; u->status.success=false; u->status.reboot_required=false;
    for(unsigned i=0;i<CAR_UPGRADE_COMPONENTS;++i) u->status.phase[i]=CAR_UPGRADE_WAITING;
    ++u->status.revision; pthread_mutex_unlock(&u->lock);
    if(!request_save(u)) {
        for(unsigned i=0;i<CAR_UPGRADE_COMPONENTS;++i) phase(u,i,CAR_UPGRADE_SAVE_FAILED);
        success=false; goto finished;
    }
    if(package->system[0]) {
        phase(u,CAR_UPGRADE_SYSTEM,CAR_UPGRADE_COPYING);
        bool changed=false;
        bool ok=car_upgrade_install_system(package->system,u->root,&changed);
        reboot|=changed; success&=ok;
        phase(u,CAR_UPGRADE_SYSTEM,ok?CAR_UPGRADE_SUCCESS:CAR_UPGRADE_FAILED);
    } else phase(u,CAR_UPGRADE_SYSTEM,CAR_UPGRADE_MISSING);
    bool app_updated=false;
    if(package->app[0] && !cancelled(u)) {
        char local[CAR_UPGRADE_PATH],hash[33];
        phase(u,CAR_UPGRADE_APP,CAR_UPGRADE_CHECKING);
        int result=join_path(local,u->home,"car_dashboard_hml.md5")?
            car_upgrade_verify(package->app,local,hash):-1;
        if(result>0) {
            phase(u,CAR_UPGRADE_APP,CAR_UPGRADE_COPYING);
            bool changed=false;
            app_updated=car_upgrade_install_app(package->app,u->home,hash,&changed);
            reboot|=changed; success&=app_updated;
            phase(u,CAR_UPGRADE_APP,app_updated?CAR_UPGRADE_SUCCESS:CAR_UPGRADE_FAILED);
        } else {
            success&=result==0;
            phase(u,CAR_UPGRADE_APP,result==0?CAR_UPGRADE_CURRENT:CAR_UPGRADE_FAILED);
        }
    } else phase(u,CAR_UPGRADE_APP,CAR_UPGRADE_MISSING);
    if(package->mcu[0] && !cancelled(u)) {
        if(app_updated) (void)delay(u,7000);
        char volume[CAR_UPGRADE_PATH]; strcpy(volume,package->mcu);
        char *slash=strrchr(volume,'/');
        if(slash) { *slash=0; success&=mcu_candidates(u,volume); }
        else success=false;
    } else phase(u,CAR_UPGRADE_MCU,CAR_UPGRADE_MISSING);
finished:
    pthread_mutex_lock(&u->lock);
    u->status.busy=false; u->status.finished=true; u->status.success=success && !cancelled(u);
    u->status.reboot_required=reboot; ++u->status.revision; pthread_mutex_unlock(&u->lock);
}
static bool detect_package(car_upgrade_t *u, car_upgrade_package_t *p)
{
    if(car_upgrade_find_usb(u->usb,p)) return true;
    /* Qt also discovers removable /dev/sd mounts outside /mnt/usb. */
    if(strcmp(u->usb,"/mnt/usb")) return false;
    FILE *mounts=fopen("/proc/mounts","r"); if(!mounts) return false;
    char line[2048],device[256],mount[CAR_UPGRADE_PATH],type[80],options[512];
    bool found=false;
    while(fgets(line,sizeof(line),mounts)) {
        if(sscanf(line,"%255s %511s %79s %511s",device,mount,type,options)!=4) continue;
        if(strncmp(device,"/dev/sd",7) || !strcmp(mount,"/mnt/resource")) continue;
        /* Decode proc mount escaping without executing path text. */
        char decoded[CAR_UPGRADE_PATH]; size_t n=0;
        for(size_t i=0;mount[i] && n+1<sizeof(decoded);++i) {
            if(mount[i]=='\\' && mount[i+1]>='0' && mount[i+1]<='7' &&
               mount[i+2]>='0' && mount[i+2]<='7' && mount[i+3]>='0' && mount[i+3]<='7') {
                decoded[n++]=(char)((mount[i+1]-'0')*64+(mount[i+2]-'0')*8+mount[i+3]-'0'); i+=3;
            } else decoded[n++]=mount[i];
        }
        decoded[n]=0;
        if(access(decoded,W_OK)==0 && car_upgrade_scan_volume(decoded,p)) { found=true; break; }
    }
    fclose(mounts); return found;
}
static void *worker_main(void *user)
{
    car_upgrade_t *u=user;
    char consumed[CAR_UPGRADE_PATH]="";
    while(!cancelled(u)) {
        car_upgrade_package_t package;
        if(detect_package(u,&package)) {
            if(strcmp(consumed,package.volume)) {
                /* Mount/automount settling delay from Qt. */
                if(!delay(u,2000)) break;
                car_upgrade_package_t settled;
                if(detect_package(u,&settled)) {
                    strcpy(consumed,settled.volume); run_package(u,&settled);
                }
            }
        } else {
            consumed[0]=0;
            pthread_mutex_lock(&u->lock);
            if(u->status.finished && !u->status.reboot_required) {
                u->status.visible=false; ++u->status.revision;
            }
            pthread_mutex_unlock(&u->lock);
        }
        (void)delay(u,500);
    }
    return NULL;
}
car_upgrade_t *car_upgrade_create(const car_upgrade_config_t *config, car_link_t *link)
{
    if(!config || !link || !config->usb_root || !config->app_home ||
       !config->system_root || !config->serial || strlen(config->usb_root)>=CAR_UPGRADE_PATH ||
       strlen(config->app_home)>=CAR_UPGRADE_PATH || strlen(config->system_root)>=CAR_UPGRADE_PATH ||
       strlen(config->serial)>=64) return NULL;
    car_upgrade_t *u=calloc(1,sizeof(*u)); if(!u) return NULL;
    u->config=*config; u->link=link;
    strcpy(u->usb,config->usb_root); strcpy(u->home,config->app_home);
    strcpy(u->root,config->system_root); strcpy(u->serial,config->serial);
    if(pthread_mutex_init(&u->lock,NULL)!=0) { free(u); return NULL; }
    char flag[CAR_UPGRADE_PATH];
    if(join_path(flag,u->home,"mcu_flag")) {
        FILE *file=fopen(flag,"rb");
        if(file) {
            u->status.interrupted_mcu=fgetc(file)=='1'; fclose(file);
            if(u->status.interrupted_mcu) {
                u->status.visible=true; u->status.phase[CAR_UPGRADE_MCU]=CAR_UPGRADE_INTERRUPTED;
                ++u->status.revision;
            }
        }
    }
    if(pthread_create(&u->worker,NULL,worker_main,u)!=0) {
        pthread_mutex_destroy(&u->lock); free(u); return NULL;
    }
    return u;
}
static void resume_link(car_upgrade_t *u)
{
    bool ok=car_link_resume(u->link);
    if(ok && u->restart_thread) ok=car_link_start(u->link);
    u->suspended=false;
    pthread_mutex_lock(&u->lock); u->status.serial_failed=!ok;
    if(!ok) ++u->status.revision;
    pthread_mutex_unlock(&u->lock);
}
void car_upgrade_poll(car_upgrade_t *u, car_upgrade_status_t *status)
{
    if(!u) { memset(status,0,sizeof(*status)); return; }
    pthread_mutex_lock(&u->lock);
    int save=u->save_request, serial=u->serial_request;
    pthread_mutex_unlock(&u->lock);
    if(save==1) {
        bool ok=!u->config.save_settings || u->config.save_settings(u->config.user);
        pthread_mutex_lock(&u->lock); u->save_request=ok?2:-1; pthread_mutex_unlock(&u->lock);
    }
    if(serial==1) {
        u->restart_thread=u->link->thread_started;
        car_link_suspend(u->link); u->suspended=true;
        pthread_mutex_lock(&u->lock); u->serial_request=2; pthread_mutex_unlock(&u->lock);
    } else if(serial==3) {
        if(u->suspended) resume_link(u);
        pthread_mutex_lock(&u->lock); u->serial_request=0; pthread_mutex_unlock(&u->lock);
    }
    pthread_mutex_lock(&u->lock); *status=u->status; pthread_mutex_unlock(&u->lock);
}
void car_upgrade_dismiss(car_upgrade_t *u)
{
    if(!u) return;
    pthread_mutex_lock(&u->lock);
    if(!u->status.busy && !u->status.reboot_required) { u->status.visible=false; ++u->status.revision; }
    pthread_mutex_unlock(&u->lock);
}
void car_upgrade_destroy(car_upgrade_t *u)
{
    if(!u) return;
    atomic_store(&u->stop,true); pthread_join(u->worker,NULL);
    if(u->suspended) resume_link(u);
    pthread_mutex_destroy(&u->lock); free(u);
}
