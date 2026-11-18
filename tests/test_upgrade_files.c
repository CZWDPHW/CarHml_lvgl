#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#include "carmw/car_upgrade_files.h"
#include "carmw/car_md5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#endif
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%d: %s (errno=%d)\n",__LINE__,#x,errno); exit(1); } } while(0)
static void directory(const char *path)
{
#ifdef _WIN32
    CHECK(_mkdir(path)==0);
#else
    CHECK(mkdir(path,0755)==0);
#endif
}
static void write_text(const char *path, const char *text)
{
    FILE *f=fopen(path,"wb"); CHECK(f); CHECK(fwrite(text,1,strlen(text),f)==strlen(text)); CHECK(fclose(f)==0);
}
static void expect(const char *path, const char *text)
{
    char buffer[1024]={0}; FILE *f=fopen(path,"rb"); CHECK(f);
    size_t n=fread(buffer,1,sizeof(buffer)-1,f); CHECK(!ferror(f)); fclose(f);
    CHECK(n==strlen(text) && !strcmp(buffer,text));
}
int main(void)
{
    /* Caller runs this executable in a fresh isolated directory. */
    directory("usb"); directory("usb/sdb1"); directory("usb/sda1");
    write_text("usb/sdb1/firmware.BIN","abc");
    write_text("usb/sdb1/firmware.BIN.md5","900150983CD24FB0D6963F7D28E17F72 *firmware.BIN\n");
    car_upgrade_package_t package;
    CHECK(car_upgrade_find_usb("usb",&package));
    CHECK(!strcmp(package.mcu,"usb/sdb1/firmware.BIN"));
    char hash[33];
    CHECK(car_upgrade_verify(package.mcu,"installed.md5",hash)==1);
    CHECK(!strcmp(hash,"900150983cd24fb0d6963f7d28e17f72"));
    CHECK(car_upgrade_atomic_text("./installed.md5",hash));
    CHECK(car_upgrade_verify(package.mcu,"installed.md5",hash)==0);
    write_text(package.mcu,"bad");
    CHECK(car_upgrade_verify(package.mcu,"installed.md5",hash)==-1);
    CHECK(car_upgrade_verify(package.mcu,"missing-first-install.md5",hash)==-1);
    CHECK(car_upgrade_copy_file("installed.md5","./copy",0600)); expect("copy",hash);
    write_text("copy.upgrade-new","pending");
    CHECK(!car_upgrade_copy_file("installed.md5","./copy",0600)); expect("copy",hash);
    CHECK(remove("copy.upgrade-new")==0);
    CHECK(car_upgrade_atomic_text("./copy","replacement")); expect("copy","replacement");
    directory("usb/sda1/system_update"); directory("usb/sda1/system_update/bin");
    directory("usb/sda1/system_update/lib"); directory("usb/sda1/system_update/etc");
    directory("usb/sda1/system_update/res"); directory("usb/sda1/system_update/share");
    directory("usb/sda1/system_update/share/fonts");
    write_text("usb/sda1/system_update/bin/tool","new binary");
    write_text("usb/sda1/system_update/lib/library.so","new library");
    write_text("usb/sda1/system_update/etc/wpa_supplicant.conf","new config");
    write_text("usb/sda1/system_update/res/bootlogo.bmp","new logo");
    write_text("usb/sda1/system_update/share/fonts/new.ttf","new font");
    directory("target"); directory("target/usr"); directory("target/usr/bin"); directory("target/usr/lib");
    directory("target/usr/share"); directory("target/usr/share/fonts");
    directory("target/etc"); directory("target/mnt"); directory("target/mnt/resource");
    write_text("target/usr/share/fonts/old.ttf","old font");
    write_text("target/mnt/resource/bootlogo1.bmp","old logo");
    CHECK(car_upgrade_find_usb("usb",&package)); CHECK(package.system[0] && package.mcu[0]);
    bool changed=false;
    CHECK(car_upgrade_install_system(package.system,"target",&changed) && changed);
    expect("target/usr/bin/tool","new binary"); expect("target/usr/lib/library.so","new library");
    expect("target/etc/wpa_supplicant.conf","new config"); expect("target/mnt/resource/bootlogo.bmp","new logo");
    expect("target/usr/share/fonts/new.ttf","new font");
    FILE *f=fopen("target/usr/share/fonts/old.ttf","rb"); CHECK(!f);
    f=fopen("target/mnt/resource/bootlogo1.bmp","rb"); CHECK(!f);
#ifndef _WIN32
    directory("home"); directory("home/assets");
    write_text("home/car_A","baseline"); write_text("home/boot_flag","0\n");
    write_text("home/assets/image.txt","baseline asset");
    write_text("usb/sda1/car_dashboard_hml","application one");
    CHECK(car_md5_file("usb/sda1/car_dashboard_hml",hash));
    CHECK(car_upgrade_install_app("usb/sda1/car_dashboard_hml","home",hash,&changed) && changed);
    expect("home/car_A","application one"); expect("home/car_B","baseline"); expect("home/boot_flag","0\n");
    char first[512],asset[600];
    CHECK(realpath("home/car_A",first));
    char *slash=strrchr(first,'/'); CHECK(slash); *slash=0;
    snprintf(asset,sizeof(asset),"%s/assets/image.txt",first); expect(asset,"baseline asset");
    directory("usb/sda1/assets"); write_text("usb/sda1/assets/image.txt","updated asset");
    write_text("usb/sda1/car_dashboard_hml","application two");
    CHECK(car_md5_file("usb/sda1/car_dashboard_hml",hash));
    CHECK(car_upgrade_install_app("usb/sda1/car_dashboard_hml","home",hash,&changed) && changed);
    expect("home/car_A","application two"); expect("home/car_B","application one");
    expect(asset,"baseline asset"); /* previous executable and resource pair survived */
    write_text("home/boot_flag","1\n");
    write_text("usb/sda1/car_dashboard_hml","application three");
    CHECK(car_md5_file("usb/sda1/car_dashboard_hml",hash));
    CHECK(car_upgrade_install_app("usb/sda1/car_dashboard_hml","home",hash,&changed) && changed);
    expect("home/car_A","application three"); expect("home/car_B","application one");
#endif
    puts("Upgrade package, checksum and atomic file tests passed"); return 0;
}
