#ifndef CARMW_CAR_UPGRADE_FILES_H
#define CARMW_CAR_UPGRADE_FILES_H
#include <stdbool.h>
#define CAR_UPGRADE_PATH 512
typedef struct {
    char volume[CAR_UPGRADE_PATH];
    char app[CAR_UPGRADE_PATH];
    char mcu[CAR_UPGRADE_PATH];
    char system[CAR_UPGRADE_PATH];
} car_upgrade_package_t;
/* All install paths are rooted explicitly, so tests cannot write into the host system. */
bool car_upgrade_scan_volume(const char *volume, car_upgrade_package_t *package);
bool car_upgrade_find_usb(const char *usb_root, car_upgrade_package_t *package);
bool car_upgrade_atomic_text(const char *path, const char *text);
bool car_upgrade_copy_file(const char *source, const char *destination, unsigned mode);
/* 1 verified new content; 0 already installed; -1 missing/invalid checksum. */
int car_upgrade_verify(const char *source, const char *installed_md5, char hash[33]);
bool car_upgrade_install_system(const char *package, const char *target_root, bool *changed);
bool car_upgrade_install_app(const char *source, const char *app_home, const char hash[33], bool *changed);
#endif
