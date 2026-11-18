#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#include "carmw/car_upgrade_files.h"
#include "carmw/car_md5.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#define sync_file _commit
#else
#define sync_file fsync
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif

static bool path_join(char out[CAR_UPGRADE_PATH], const char *a, const char *b)
{
    int n=snprintf(out,CAR_UPGRADE_PATH,"%s/%s",a,b);
    return n>0 && n<CAR_UPGRADE_PATH;
}
static bool plain(const char *path, bool directory)
{
    struct stat st;
#ifdef _WIN32
    DWORD attr=GetFileAttributesA(path);
    if(attr==INVALID_FILE_ATTRIBUTES || (attr&FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    if(stat(path,&st)!=0) return false;
#else
    if(lstat(path,&st)!=0 || S_ISLNK(st.st_mode)) return false;
#endif
    return directory ? S_ISDIR(st.st_mode) : S_ISREG(st.st_mode);
}
static bool make_dir(const char *path)
{
    if(plain(path,true)) return true;
#ifdef _WIN32
    return _mkdir(path)==0;
#else
    return mkdir(path,0755)==0;
#endif
}
static bool sync_parent(const char *path)
{
#ifdef _WIN32
    (void)path; return true; /* Windows test backend; BOARD always uses POSIX. */
#else
    char parent[CAR_UPGRADE_PATH];
    if(strlen(path)>=sizeof(parent)) return false;
    strcpy(parent,path); char *p=strrchr(parent,'/'); if(!p) return false;
    *p=0; int fd=open(parent[0]?parent:"/",O_RDONLY|O_DIRECTORY);
    if(fd<0) return false;
    bool ok=fsync(fd)==0; close(fd); return ok;
#endif
}
static bool replace(const char *source, const char *destination)
{
#ifdef _WIN32
    return MoveFileExA(source,destination,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    return rename(source,destination)==0 && sync_parent(destination);
#endif
}
static bool finish_file(FILE *file)
{
    bool ok=fflush(file)==0;
    if(sync_file(fileno(file))!=0) ok=false;
    if(fclose(file)!=0) ok=false;
    return ok;
}
bool car_upgrade_atomic_text(const char *path, const char *text)
{
    char temp[CAR_UPGRADE_PATH];
    int n=snprintf(temp,sizeof(temp),"%s.upgrade-new",path);
    if(n<0 || n>=(int)sizeof(temp)) return false;
    /* Never follow a package-controlled or stale temporary symlink. */
    FILE *probe=fopen(temp,"rb"); if(probe) { fclose(probe); return false; }
    int fd=open(temp,O_WRONLY|O_CREAT|O_EXCL|O_BINARY,0644); if(fd<0) return false;
    FILE *file=fdopen(fd,"wb"); if(!file) { close(fd); remove(temp); return false; }
    bool ok=fwrite(text,1,strlen(text),file)==strlen(text);
    if(!finish_file(file)) ok=false;
    if(ok) ok=replace(temp,path);
    if(!ok) remove(temp);
    return ok;
}
bool car_upgrade_copy_file(const char *source, const char *destination, unsigned mode)
{
    if(!plain(source,false)) return false;
    char temp[CAR_UPGRADE_PATH];
    int n=snprintf(temp,sizeof(temp),"%s.upgrade-new",destination);
    if(n<0 || n>=(int)sizeof(temp)) return false;
    FILE *in=fopen(source,"rb"); if(!in) return false;
    int fd=open(temp,O_WRONLY|O_CREAT|O_EXCL|O_BINARY,mode);
    if(fd<0) { fclose(in); return false; }
    FILE *out=fdopen(fd,"wb");
    if(!out) { close(fd); fclose(in); remove(temp); return false; }
    uint8_t block[16384]; size_t count; bool ok=true;
    while((count=fread(block,1,sizeof(block),in))!=0)
        if(fwrite(block,1,count,out)!=count) { ok=false; break; }
    if(ferror(in)) ok=false;
    fclose(in);
    if(chmod(temp,(int)mode)!=0) ok=false;
    if(!finish_file(out)) ok=false;
    if(ok) ok=replace(temp,destination);
    if(!ok) remove(temp);
    return ok;
}
static bool suffix(const char *name, const char *tail)
{
    size_t n=strlen(name), t=strlen(tail);
    if(n<t) return false;
    for(size_t i=0;i<t;++i) {
        char c=name[n-t+i]; if(c>='A' && c<='Z') c=(char)(c+'a'-'A');
        if(c!=tail[i]) return false;
    }
    return true;
}
static bool valid_entry(const char *name)
{
    return name[0] && strcmp(name,".") && strcmp(name,"..") &&
           !strchr(name,'/') && !strchr(name,'\\') && !strchr(name,'\n') && !strchr(name,'\r');
}
bool car_upgrade_scan_volume(const char *volume, car_upgrade_package_t *p)
{
    memset(p,0,sizeof(*p));
    if(strlen(volume)>=sizeof(p->volume) || !plain(volume,true)) return false;
    strcpy(p->volume,volume); char path[CAR_UPGRADE_PATH];
    if(path_join(path,volume,"car_dashboard_hml") && plain(path,false)) strcpy(p->app,path);
    if(path_join(path,volume,"system_update") && plain(path,true)) strcpy(p->system,path);
    DIR *d=opendir(volume); if(!d) return false;
    struct dirent *e;
    while((e=readdir(d))) {
        if(!valid_entry(e->d_name) || !suffix(e->d_name,".bin")) continue;
        if(path_join(path,volume,e->d_name) && plain(path,false) &&
           (!p->mcu[0] || strcmp(path,p->mcu)<0)) strcpy(p->mcu,path);
    }
    closedir(d); return p->app[0] || p->mcu[0] || p->system[0];
}
bool car_upgrade_find_usb(const char *root, car_upgrade_package_t *p)
{
    memset(p,0,sizeof(*p)); DIR *d=opendir(root); if(!d) return false;
    struct dirent *e; char path[CAR_UPGRADE_PATH]; car_upgrade_package_t candidate;
    while((e=readdir(d))) {
        if(!valid_entry(e->d_name) || !path_join(path,root,e->d_name)) continue;
        if(car_upgrade_scan_volume(path,&candidate)) {
            if(!p->volume[0] || strcmp(candidate.volume,p->volume)<0) strcpy(p->volume,candidate.volume);
            if(candidate.app[0] && (!p->app[0] || strcmp(candidate.app,p->app)<0)) strcpy(p->app,candidate.app);
            if(candidate.mcu[0] && (!p->mcu[0] || strcmp(candidate.mcu,p->mcu)<0)) strcpy(p->mcu,candidate.mcu);
            if(candidate.system[0] && (!p->system[0] || strcmp(candidate.system,p->system)<0)) strcpy(p->system,candidate.system);
        }
    }
    closedir(d); return p->volume[0]!=0;
}
int car_upgrade_verify(const char *source, const char *local, char hash[33])
{
    char sidecar[CAR_UPGRADE_PATH], actual[33], installed[33];
    int n=snprintf(sidecar,sizeof(sidecar),"%s.md5",source);
    if(n<0 || n>=(int)sizeof(sidecar) || !car_md5_read_sidecar(sidecar,hash) ||
       !car_md5_file(source,actual) || strcmp(hash,actual)) return -1;
    if(local && car_md5_read_sidecar(local,installed) && !strcmp(installed,hash)) return 0;
    return 1;
}
/* Recursive package trees contain regular files and directories only. */
static bool tree_copy(const char *source, const char *destination, unsigned default_mode)
{
    if(!plain(source,true) || !make_dir(destination)) return false;
    DIR *d=opendir(source); if(!d) return false;
    bool ok=true; struct dirent *e; char from[CAR_UPGRADE_PATH],to[CAR_UPGRADE_PATH];
    while((e=readdir(d))) {
        if(!strcmp(e->d_name,".") || !strcmp(e->d_name,"..")) continue;
        if(!valid_entry(e->d_name) || !path_join(from,source,e->d_name) ||
           !path_join(to,destination,e->d_name)) { ok=false; break; }
        if(plain(from,true)) { if(!tree_copy(from,to,default_mode)) ok=false; }
        else {
            unsigned mode=default_mode;
            struct stat st;
            if(stat(to,&st)==0) mode=(unsigned)st.st_mode&0777U;
            else if(strstr(to,"/etc/init.d/")) mode=0755;
            else if(strstr(to,"wpa_supplicant")) mode=0600;
            if(!car_upgrade_copy_file(from,to,mode)) ok=false;
        }
    }
    closedir(d); return ok && sync_parent(destination);
}
static bool remove_regular_tree(const char *root)
{
    DIR *d=opendir(root); if(!d) return errno==ENOENT;
    struct dirent *e; bool ok=true; char path[CAR_UPGRADE_PATH];
    while((e=readdir(d))) {
        if(!strcmp(e->d_name,".") || !strcmp(e->d_name,"..")) continue;
        if(!valid_entry(e->d_name) || !path_join(path,root,e->d_name)) { ok=false; break; }
        if(plain(path,true)) { if(!remove_regular_tree(path)) ok=false; }
        else if(!plain(path,false) || remove(path)!=0) ok=false;
    }
    closedir(d); return ok;
}
bool car_upgrade_install_system(const char *package, const char *root, bool *changed)
{
    static const char *const from[]={"bin","lib","share/fonts","etc","res"};
    static const char *const to[]={"usr/bin","usr/lib","usr/share/fonts","etc","mnt/resource"};
    bool ok=true, any=false; char source[CAR_UPGRADE_PATH],target[CAR_UPGRADE_PATH];
    *changed=false;
    for(unsigned i=0;i<5;++i) {
        if(!path_join(source,package,from[i]) || !path_join(target,root,to[i])) return false;
        struct stat st; if(stat(source,&st)!=0 && errno==ENOENT) continue;
        any=true;
        /* A partially installed component still needs a reboot; never hide that fact. */
        *changed=true;
        if(i==2 && !remove_regular_tree(target)) { ok=false; continue; }
        if(i==4) {
            char logo[CAR_UPGRADE_PATH];
            if(path_join(logo,target,"bootlogo1.bmp") && plain(logo,false) && remove(logo)!=0) ok=false;
        }
        if(!tree_copy(source,target,i==0?0755:0644)) ok=false;
    }
    return any && ok;
}
#ifndef _WIN32
static bool publish_link(const char *target, const char *link)
{
    char temp[CAR_UPGRADE_PATH];
    int n=snprintf(temp,sizeof(temp),"%s.upgrade-new",link);
    if(n<0 || n>=(int)sizeof(temp) || symlink(target,temp)!=0) return false;
    if(!replace(temp,link)) { unlink(temp); return false; }
    return true;
}
#endif
bool car_upgrade_install_app(const char *source, const char *home, const char hash[33], bool *changed)
{
    *changed=false;
#ifdef _WIN32
    (void)source; (void)home; (void)hash; return false; /* symlink transaction requires Linux */
#else
    char canonical_home[CAR_UPGRADE_PATH];
    char *resolved_home=realpath(home,NULL);
    if(!resolved_home || strlen(resolved_home)>=sizeof(canonical_home)) { free(resolved_home); return false; }
    strcpy(canonical_home,resolved_home); free(resolved_home); home=canonical_home;
    char releases[CAR_UPGRADE_PATH], release[CAR_UPGRADE_PATH], executable[CAR_UPGRADE_PATH];
    char a[CAR_UPGRADE_PATH],b[CAR_UPGRADE_PATH],flag[CAR_UPGRADE_PATH],md5[CAR_UPGRADE_PATH];
    char old[CAR_UPGRADE_PATH],assets[CAR_UPGRADE_PATH],new_assets[CAR_UPGRADE_PATH];
    char package[CAR_UPGRADE_PATH],actual[33];
    if(!path_join(releases,home,"releases") || !make_dir(releases) ||
       !path_join(release,releases,hash) || !make_dir(release) ||
       !path_join(executable,release,"car_dashboard_hml") ||
       !path_join(a,home,"car_A") || !path_join(b,home,"car_B") ||
       !path_join(flag,home,"boot_flag") || !path_join(md5,home,"car_dashboard_hml.md5")) return false;
    /* Resolve the active application before changing either A/B name. */
    FILE *f=fopen(flag,"rb"); int active=f?fgetc(f):'0'; if(f) fclose(f);
    char *resolved=realpath(active=='1'?b:a,NULL);
    if(!resolved || strlen(resolved)>=sizeof(old)) { free(resolved); return false; }
    strcpy(old,resolved); free(resolved);
    if(!car_upgrade_copy_file(source,executable,0755) ||
       !car_md5_file(executable,actual) || strcmp(actual,hash)) return false;
    if(strlen(source)>=sizeof(package)) return false;
    strcpy(package,source); char *slash=strrchr(package,'/'); if(!slash) return false; *slash=0;
    if(!path_join(assets,package,"assets") || !path_join(new_assets,release,"assets")) return false;
    if(!plain(assets,true)) {
        /* Binary-only Qt packages remain usable when LVGL resources did not change. */
        char base[CAR_UPGRADE_PATH]; strcpy(base,old); slash=strrchr(base,'/'); *slash=0;
        if(!path_join(assets,base,"assets") || !plain(assets,true)) return false;
    }
    if(!tree_copy(assets,new_assets,0644)) return false;
    if(!strcmp(old,a) || !strcmp(old,b)) {
        /* Legacy regular car_A must have an immutable backup before replacing its name. */
        char baseline[CAR_UPGRADE_PATH];
        if(!path_join(baseline,home,"car_baseline") ||
           !car_upgrade_copy_file(old,baseline,0755)) return false;
        strcpy(old,baseline);
    }
    /* Old executable and its cwd/assets stay valid throughout publication. */
    if(!publish_link(old,b) || !car_upgrade_atomic_text(flag,"1\n")) return false;
    *changed=true;
    if(!publish_link(executable,a) || !car_upgrade_atomic_text(flag,"0\n")) return false;
    return car_upgrade_atomic_text(md5,hash);
#endif
}
