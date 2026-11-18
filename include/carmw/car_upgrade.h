#ifndef CARMW_CAR_UPGRADE_H
#define CARMW_CAR_UPGRADE_H
#include "carmw/car_link.h"
#include "carmw/car_upgrade_files.h"
typedef enum { CAR_UPGRADE_APP, CAR_UPGRADE_MCU, CAR_UPGRADE_SYSTEM, CAR_UPGRADE_COMPONENTS } car_upgrade_component_t;
typedef enum {
    CAR_UPGRADE_IDLE, CAR_UPGRADE_WAITING, CAR_UPGRADE_CHECKING, CAR_UPGRADE_COPYING,
    CAR_UPGRADE_PREPARING, CAR_UPGRADE_TRANSFERRING, CAR_UPGRADE_SUCCESS,
    CAR_UPGRADE_CURRENT, CAR_UPGRADE_MISSING, CAR_UPGRADE_FAILED,
    CAR_UPGRADE_INTERRUPTED, CAR_UPGRADE_SAVE_FAILED
} car_upgrade_phase_t;
typedef struct {
    car_upgrade_phase_t phase[CAR_UPGRADE_COMPONENTS];
    unsigned mcu_percent;
    bool visible, busy, finished, success, reboot_required, interrupted_mcu;
    bool serial_failed;
    unsigned revision;
} car_upgrade_status_t;
typedef struct car_upgrade car_upgrade_t;
typedef struct {
    const char *usb_root;   /* /mnt/usb */
    const char *app_home;   /* /opt/car */
    const char *system_root;/* / */
    const char *serial;     /* /dev/ttyS2 */
    unsigned baud;         /* 115200 */
    bool (*save_settings)(void *user); /* Called on UI thread before starting an update. */
    void *user;
} car_upgrade_config_t;
/* Linux worker; poll and destroy must be called on the owner/UI thread. */
car_upgrade_t *car_upgrade_create(const car_upgrade_config_t *config, car_link_t *link);
void car_upgrade_poll(car_upgrade_t *upgrade, car_upgrade_status_t *status);
void car_upgrade_destroy(car_upgrade_t *upgrade);
/* Hide a finished result only; active upgrades consume all keys. */
void car_upgrade_dismiss(car_upgrade_t *upgrade);
#endif
