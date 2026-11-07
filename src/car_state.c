#include "carmw/car_state.h"

#include <string.h>

void car_state_init(car_state_t *state)
{
    if(state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->coolant.display = CAR_COOLANT_SHOW_VALUE;
    state->gear.text[0] = '\0';
    /* 车身颜色属设置项而非串口数据，缺省取 Qt SettingsManager 的默认值 */
    state->car_color = car_color_default();
    /* Qt SettingsMgr.outTemperature 缺省 false，车外温度默认不显示 */
    state->outside_temp_on = false;
    /* Qt SettingsMgr.gearOnOff 缺省 true */
    state->gear_on = true;
    /* Qt SettingsMgr.ecoIndicator 缺省 true */
    state->eco_indicator = true;
    /* Qt GlobalData.items 白天/黑夜亮度缺省 100 */
    state->daylight = 100U;
    state->nightlight = 100U;
    /* Qt SettingsMgr 三项驾驶辅助缺省 true，尚未收到 MCU 回读 */
    state->driver_assist.emergency_braking = true;
    state->driver_assist.lane_departure_warning = true;
    state->driver_assist.blind_spot_warning = true;
    state->driver_assist.valid = false;
    /* Qt SettingsMgr 车辆设置项缺省：开关为 true，选项为 0 */
    state->vehicle_setting.welcome_light = true;
    state->vehicle_setting.auto_interior_light = true;
    state->vehicle_setting.external_doors = true;
    state->vehicle_setting.single_door_unlock = true;
    state->vehicle_setting.horn_feedback = true;
    state->vehicle_setting.auto_door_unlock = 0U;
    state->vehicle_setting.smart_door_unlock = true;
    state->vehicle_setting.select_unlock = true;
    state->vehicle_setting.auto_door_lock = 0U;
    state->vehicle_setting.headlight_sensitivity = 0U;
    state->vehicle_setting.headlight_off_delay = 0U;
    state->vehicle_setting.valid = false;
    /* Qt radaronoff 实为雷达开启，缺省 true；show4h 缺省 false */
    state->radar_on = true;
    state->show_4h = false;
    state->carplay_active = false;
}

uint8_t car_state_warning_level(const car_state_t *state, uint8_t code)
{
    if(state == NULL) {
        return (uint8_t)CAR_WARN_OFF;
    }

    return state->warnings.levels[code];
}

car_gear_pos_t car_gear_from_text(const char *text, car_gear_pos_t fallback)
{
    if(text == NULL) {
        return fallback;
    }

    switch(text[0]) {
        case 'P':
            return CAR_GEAR_P;
        case 'R':
            return CAR_GEAR_R;
        case 'N':
            return CAR_GEAR_N;
        case 'D':
            return CAR_GEAR_D;
        case 'S':
            return CAR_GEAR_S;
        default:
            return fallback;
    }
}

void car_state_warn_event_push(car_state_t *state, uint16_t text_id)
{
    car_warn_events_t *q;
    uint8_t slot;

    if(state == NULL) {
        return;
    }

    q = &state->warn_events;

    /* 满队列：丢掉最旧一条，保留最近事件，序号继续单调递增 */
    if(q->count >= CAR_WARN_EVENT_QUEUE_MAX) {
        q->head = (uint8_t)((q->head + 1U) % CAR_WARN_EVENT_QUEUE_MAX);
        q->count = (uint8_t)(q->count - 1U);
        q->dropped++;
    }

    slot = (uint8_t)((q->head + q->count) % CAR_WARN_EVENT_QUEUE_MAX);
    q->items[slot].text_id = text_id;
    q->items[slot].seq = ++q->seq;
    q->count = (uint8_t)(q->count + 1U);
}

void car_state_store_init(car_state_store_t *store)
{
    if(store == NULL) {
        return;
    }

    car_lock_init(&store->lock);
    car_state_init(&store->state);
    store->changed = 0;
    store->version = 0;
    store->has_data = false;
}

void car_state_store_deinit(car_state_store_t *store)
{
    if(store == NULL) {
        return;
    }

    car_lock_destroy(&store->lock);
}
/**
 * @brief 应用新的状态数据
 * @param store 状态存储器
 * @param sample 样本状态
 * @param mask 变更掩码
 */
void car_state_store_apply(car_state_store_t *store,
                           const car_state_t *sample,
                           uint64_t mask)
{
    if(store == NULL || sample == NULL) {
        return;
    }

    car_lock_acquire(&store->lock);
    store->state = *sample;// 应用新的状态数据
    store->changed |= mask;// 记录变更掩码
    store->version++;// 版本号增加
    store->has_data = true;// 标记有数据
    car_lock_release(&store->lock);
}

/**
 * @brief 获取状态视图
 * @param store 状态存储器
 * @param view 视图指针
 */
void car_state_store_view(car_state_store_t *store, car_state_view_t *view)
{
    if(store == NULL || view == NULL) {
        return;
    }

    car_lock_acquire(&store->lock);
    view->state = store->state;// 复制状态数据
    view->changed = store->changed;// 复制变更掩码
    view->version = store->version;// 复制版本号
    view->has_data = store->has_data;// 复制是否有数据
    store->changed = 0;// 重置变更掩码
    car_lock_release(&store->lock);
}
