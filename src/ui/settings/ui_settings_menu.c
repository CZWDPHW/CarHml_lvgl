/* 菜单相当于 QML ListModel：数组顺序就是显示顺序。
 * 删入口只删这里的一行；绑定、MCU index、翻译都不用随之调整。
 * 最后保留 BACK；行数由 sizeof 自动计算，不要手写 COUNT。 */
#include "ui_settings_menu.h"
#include <stddef.h>

static const ui_settings_item_id_t itinerary_menu[] = {
    UI_SETTINGS_IT_TRIP,
    UI_SETTINGS_IT_ASSIST,
    UI_SETTINGS_IT_VEHICLE,
    UI_SETTINGS_ITEM_ROOT_BACK,
};

static const ui_settings_item_id_t assist_menu[] = {
    UI_SETTINGS_ASSIST_DRIVING,
    UI_SETTINGS_ASSIST_PARKING,
    UI_SETTINGS_ASSIST_CHASSIS,
    UI_SETTINGS_ASSIST_EPS,
    UI_SETTINGS_ITEM_BACK,
};

static const ui_settings_item_id_t eps_menu[] = {
    UI_SETTINGS_EPS_MODE,
    UI_SETTINGS_ITEM_BACK,
};

static const ui_settings_item_id_t parking_menu[] = {
    UI_SETTINGS_PARK_MOVING_OBJECT,
    UI_SETTINGS_PARK_FRONT_RADAR,
    UI_SETTINGS_PARK_REAR_RADAR,
    UI_SETTINGS_PARK_DISPLAY,
    UI_SETTINGS_PARK_REAR_CROSS,
    UI_SETTINGS_PARK_VOLUME,
    UI_SETTINGS_PARK_DISTANCE,
    UI_SETTINGS_PARK_SENSOR,
    UI_SETTINGS_ITEM_BACK,
};

static const ui_settings_item_id_t chassis_menu[] = {
    UI_SETTINGS_CHASSIS_TRACE,
    UI_SETTINGS_CHASSIS_BRAKE,
    UI_SETTINGS_ITEM_BACK,
};

static const ui_settings_item_id_t driving_assist_menu[] = {
    UI_SETTINGS_DA_EMERGENCY,
    UI_SETTINGS_DA_LANE,
    UI_SETTINGS_DA_BLIND,
    UI_SETTINGS_ITEM_BACK,
};

static const ui_settings_item_id_t driving_info_menu[] = {
    UI_SETTINGS_DRV_OUT_TEMP,
    UI_SETTINGS_DRV_GEAR,
    UI_SETTINGS_DRV_AVG_FUEL,
    UI_SETTINGS_ITEM_BACK,
};

static const ui_settings_item_id_t eco_menu[] = {
    UI_SETTINGS_ECO_DISPLAY,
    UI_SETTINGS_ECO_INDICATOR,
    UI_SETTINGS_ITEM_BACK,
};

static const ui_settings_item_id_t turn_menu[] = {
    UI_SETTINGS_TURN_LANE_CHANGE,
    UI_SETTINGS_ITEM_BACK,
};

static const ui_settings_item_id_t wiper_menu[] = {
    UI_SETTINGS_WIPER_SPEED_SENSE,
    UI_SETTINGS_WIPER_REAR_LINKAGE,
    UI_SETTINGS_WIPER_MIST,
    UI_SETTINGS_ITEM_BACK,
};

#define MENU_COUNT(a) (sizeof(a) / sizeof((a)[0]))
_Static_assert(MENU_COUNT(itinerary_menu) < UI_SETTINGS_MENU_INDEX_NONE, "itinerary menu too long");
_Static_assert(MENU_COUNT(assist_menu) < UI_SETTINGS_MENU_INDEX_NONE, "assist_menu menu too long");
_Static_assert(MENU_COUNT(eps_menu) < UI_SETTINGS_MENU_INDEX_NONE, "eps menu too long");
_Static_assert(MENU_COUNT(parking_menu) < UI_SETTINGS_MENU_INDEX_NONE, "parking menu too long");
_Static_assert(MENU_COUNT(chassis_menu) < UI_SETTINGS_MENU_INDEX_NONE, "chassis menu too long");
_Static_assert(MENU_COUNT(driving_assist_menu) < UI_SETTINGS_MENU_INDEX_NONE, "driving_assist menu too long");
_Static_assert(MENU_COUNT(driving_info_menu) < UI_SETTINGS_MENU_INDEX_NONE, "driving_info menu too long");
_Static_assert(MENU_COUNT(eco_menu) < UI_SETTINGS_MENU_INDEX_NONE, "eco menu too long");
_Static_assert(MENU_COUNT(turn_menu) < UI_SETTINGS_MENU_INDEX_NONE, "turn menu too long");
_Static_assert(MENU_COUNT(wiper_menu) < UI_SETTINGS_MENU_INDEX_NONE, "wiper menu too long");

static const ui_settings_menu_t menus[UI_SETTINGS_MENU_COUNT] = {
    [UI_SETTINGS_MENU_ITINERARY] = {itinerary_menu, MENU_COUNT(itinerary_menu)},
    [UI_SETTINGS_MENU_ASSIST] = {assist_menu, MENU_COUNT(assist_menu)},
    [UI_SETTINGS_MENU_EPS] = {eps_menu, MENU_COUNT(eps_menu)},
    [UI_SETTINGS_MENU_PARKING] = {parking_menu, MENU_COUNT(parking_menu)},
    [UI_SETTINGS_MENU_CHASSIS] = {chassis_menu, MENU_COUNT(chassis_menu)},
    [UI_SETTINGS_MENU_DRIVING_ASSIST] = {driving_assist_menu, MENU_COUNT(driving_assist_menu)},
    [UI_SETTINGS_MENU_DRIVING_INFO] = {driving_info_menu, MENU_COUNT(driving_info_menu)},
    [UI_SETTINGS_MENU_ECO] = {eco_menu, MENU_COUNT(eco_menu)},
    [UI_SETTINGS_MENU_TURN] = {turn_menu, MENU_COUNT(turn_menu)},
    [UI_SETTINGS_MENU_WIPER] = {wiper_menu, MENU_COUNT(wiper_menu)},
};

const ui_settings_menu_t *ui_settings_menu_get(ui_settings_menu_id_t menu)
{
    return (unsigned)menu < UI_SETTINGS_MENU_COUNT ? &menus[menu] : NULL;
}

uint8_t ui_settings_menu_index(ui_settings_menu_id_t menu, ui_settings_item_id_t item)
{
    const ui_settings_menu_t *definition = ui_settings_menu_get(menu);
    if(definition) {
        for(uint8_t i = 0; i < definition->count; ++i)
            if(definition->items[i] == item) return i;
    }
    return UI_SETTINGS_MENU_INDEX_NONE;
}
