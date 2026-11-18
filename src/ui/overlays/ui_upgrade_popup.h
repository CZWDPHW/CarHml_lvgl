#ifndef CARMW_UI_UPGRADE_POPUP_H
#define CARMW_UI_UPGRADE_POPUP_H
#include "carmw/car_upgrade.h"
#include "lvgl.h"
/* UI thread only. Qt UpgradePopup: 800x580, modal, no automatic key dismissal. */
void ui_upgrade_popup_update(const car_upgrade_status_t *status, uint8_t language, unsigned reboot_seconds);
void ui_upgrade_popup_destroy(void);
bool ui_upgrade_popup_active(void);
void ui_upgrade_popup_restart_failed(void);
lv_obj_t *ui_upgrade_popup_root(void);
#endif
