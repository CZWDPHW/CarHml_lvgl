/* 通用列表引擎。菜单编辑 ui_settings_menu.c，功能编辑 ui_settings_bindings.inc。
 * 作为 ui_settings_page.c 的内部实现包含，复用已有 MCU 回读缓存和控件。 */
#include <stddef.h>

static lv_obj_t *make_label(lv_obj_t *, const char *, const lv_font_t *, uint32_t);
static lv_obj_t *make_row(lv_obj_t *, int32_t, int32_t, int32_t, int32_t, const char *);
static lv_obj_t *make_list(lv_obj_t *);
static void row_style(lv_obj_t *, bool, bool);
static bool feature_on(unsigned);
static void vehicle_send(uint8_t, uint8_t);

typedef enum {
    MODEL_INVALID, MODEL_MCU_SWITCH, MODEL_MCU_CHOICE, MODEL_LOCAL, MODEL_NAV, MODEL_LOCKED,
    MODEL_BACK, MODEL_ROOT_BACK
} model_kind_t;
typedef struct {
    ui_settings_text_id_t text_id;
    model_kind_t kind;
    const bool *bool_value;
    const uint8_t *number_value;
    uint8_t command_index;
    uint8_t feature_bit;
    ui_settings_view_t target_view;
    size_t local_offset;
    bool local_default;
    bool (*set_local)(car_settings_t *, bool);
    const ui_settings_text_id_t *options;
    uint8_t option_count;
} model_item_t;

#define MODEL_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#include "ui_settings_bindings.inc"

typedef struct {
    ui_settings_view_t view;
    ui_settings_menu_id_t menu;
    ui_settings_text_id_t title;
    size_t list_offset, rows_offset, index_offset;
} model_page_t;
#define MODEL_PAGE(view_id, name, title_id, list, rows, index) \
    {view_id, UI_SETTINGS_MENU_##name, title_id, \
     offsetof(ui_settings_page_t, list), offsetof(ui_settings_page_t, rows), \
     offsetof(ui_settings_state_t, index)}
static const model_page_t model_pages[] = {
    MODEL_PAGE(UI_SETTINGS_VIEW_ROOT, ITINERARY, UI_TEXT_NONE, it_list, it_rows, it_index),
    MODEL_PAGE(UI_SETTINGS_VIEW_ITINERARY_ASSIST, ASSIST, UI_TEXT_IT_ASSIST, assist_list, assist_rows, assist_index),
    MODEL_PAGE(UI_SETTINGS_VIEW_EPS, EPS, UI_TEXT_EPS_MODE, eps_list, eps_rows, eps_index),
    MODEL_PAGE(UI_SETTINGS_VIEW_PARKING, PARKING, UI_TEXT_ASSIST_PARKING, parking_list, parking_rows, parking_index),
    MODEL_PAGE(UI_SETTINGS_VIEW_CHASSIS, CHASSIS, UI_TEXT_ASSIST_CHASSIS, chassis_list, chassis_rows, chassis_index),
    MODEL_PAGE(UI_SETTINGS_VIEW_DRIVING_ASSIST, DRIVING_ASSIST, UI_TEXT_ASSIST_DRIVING, drv_assist_list, drv_assist_rows, driving_assist_index),
    MODEL_PAGE(UI_SETTINGS_VIEW_DRIVING_INFO, DRIVING_INFO, UI_TEXT_DRIVING_INFO, drv_list, drv_rows, drv_index),
    MODEL_PAGE(UI_SETTINGS_VIEW_ECO, ECO, UI_TEXT_ECO_SETTINGS, eco_list, eco_rows, eco_index),
    MODEL_PAGE(UI_SETTINGS_VIEW_TURN, TURN, UI_TEXT_TURN_LIGHT, turn_list, turn_rows, turn_index),
    MODEL_PAGE(UI_SETTINGS_VIEW_WIPER, WIPER, UI_TEXT_WIPER, wiper_list, wiper_rows, wiper_index),
};
#undef MODEL_PAGE

static const model_page_t *model_find(ui_settings_view_t view)
{
    for(size_t i = 0; i < MODEL_COUNT(model_pages); ++i)
        if(model_pages[i].view == view) return &model_pages[i];
    return NULL;
}

static const ui_settings_menu_t *model_menu(const model_page_t *model)
{
    return ui_settings_menu_get(model->menu);
}

static const model_item_t *model_item(const model_page_t *model, uint8_t row)
{
    return &model_bindings[model_menu(model)->items[row]];
}

static uint8_t model_language(void)
{
    return s_settings ? s_settings->values.language : 1U;
}

static lv_obj_t **model_rows(ui_settings_page_t *page, const model_page_t *model)
{
    return *(lv_obj_t ***)((uint8_t *)page + model->rows_offset);
}

static uint8_t *model_index(ui_settings_state_t *state, const model_page_t *model)
{
    return (uint8_t *)state + model->index_offset;
}

static bool model_item_visible(const model_item_t *item)
{
    return item->feature_bit == 0U || feature_on(item->feature_bit);
}

static bool model_item_selectable(const model_item_t *item)
{
    return item->kind != MODEL_INVALID && item->kind != MODEL_LOCKED && model_item_visible(item);
}

static bool model_selection_enabled(const ui_settings_state_t *state, const model_page_t *model)
{
    uint8_t index = *((const uint8_t *)state + model->index_offset);
    return index >= model_menu(model)->count || model_item_selectable(model_item(model, index));
}

static void model_visibility(ui_settings_page_t *page)
{
    for(size_t m = 0; m < MODEL_COUNT(model_pages); ++m) {
        const model_page_t *model = &model_pages[m];
        lv_obj_t **rows = model_rows(page, model);
        for(uint8_t i = 0; i < model_menu(model)->count; ++i) {
            if(model_item_visible(model_item(model, i))) lv_obj_remove_flag(rows[i], LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(rows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static uint8_t model_value(const model_item_t *item)
{
    if(item->kind == MODEL_MCU_SWITCH) return *item->bool_value ? 1U : 0U;
    if(item->kind == MODEL_MCU_CHOICE) return *item->number_value;
    if(item->kind == MODEL_LOCAL) {
        return s_settings ? *((const uint8_t *)&s_settings->values + item->local_offset)
                          : (uint8_t)item->local_default;
    }
    return 0U;
}

static void model_reset_index(ui_settings_state_t *state, ui_settings_view_t view)
{
    uint8_t *index = NULL;
    switch(view) {
    case UI_SETTINGS_VIEW_DRIVING_ASSIST: index = &state->driving_assist_index; break;
    case UI_SETTINGS_VIEW_EPS: index = &state->eps_index; break;
    case UI_SETTINGS_VIEW_PARKING: index = &state->parking_index; break;
    case UI_SETTINGS_VIEW_CHASSIS: index = &state->chassis_index; break;
    case UI_SETTINGS_VIEW_VEHICLE: index = &state->vehicle_index; break;
    default: break;
    }
    if(index) *index = NONE;
}

static uint8_t model_parent_item(const model_page_t *model, ui_settings_view_t child)
{
    for(uint8_t i = 0; i < model_menu(model)->count; ++i)
        if(model_item(model, i)->kind == MODEL_NAV && model_item(model, i)->target_view == child)
            return i;
    return NONE;
}

static bool model_create(ui_settings_page_t *page, ui_settings_view_t view)
{
    const model_page_t *model = model_find(view);
    uint8_t count = model_menu(model)->count;
    lv_obj_t **allocated = count ? lv_malloc_zeroed(count * sizeof(*allocated)) : NULL;
    if(count && !allocated) return false;
    *(lv_obj_t ***)((uint8_t *)page + model->rows_offset) = allocated;
    for(uint8_t i = 0; i < count; ++i) {
        ui_settings_item_id_t id = model_menu(model)->items[i];
        if(id <= UI_SETTINGS_ITEM_INVALID || id >= UI_SETTINGS_ITEM_COUNT ||
           model_bindings[id].kind == MODEL_INVALID) return false;
    }
    lv_obj_t *list = make_list(page->body);
    *(lv_obj_t **)((uint8_t *)page + model->list_offset) = list;
    lv_obj_t **rows = model_rows(page, model);
    int32_t row_height = view == UI_SETTINGS_VIEW_ROOT ? ROW_H : UNIT_ROW_H;
    for(uint8_t i = 0; i < model_menu(model)->count; ++i) {
        rows[i] = make_row(list, LIST_X, row_height * (int32_t)i,
                           LIST_W, row_height, ui_settings_text_get(model_item(model, i)->text_id, 1U));
        if(model_item(model, i)->kind == MODEL_MCU_SWITCH ||
           model_item(model, i)->kind == MODEL_MCU_CHOICE ||
           model_item(model, i)->kind == MODEL_LOCAL) {
            lv_obj_t *name = lv_obj_get_child(rows[i], 0);
            lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_LEFT, 0);
            (void)make_label(rows[i], "", UI_FONT_LG, UI_COLOR_WHITE);
        }
    }
    return true;
}

static void model_render(ui_settings_page_t *page, ui_settings_view_t view)
{
    const model_page_t *model = model_find(view);
    lv_obj_t *list = *(lv_obj_t **)((uint8_t *)page + model->list_offset);
    if(lv_obj_has_flag(list, LV_OBJ_FLAG_HIDDEN)) return;
    lv_obj_t **rows = model_rows(page, model);
    uint8_t selected = *model_index(&page->st, model);
    for(uint8_t i = 0; i < model_menu(model)->count; ++i) {
        const model_item_t *item = model_item(model, i);
        lv_obj_t *name = lv_obj_get_child(rows[i], 0);
        bool visible = model_item_visible(item);
        ui_settings_label_set_id(name, item->text_id, model_language());
        if(visible) lv_obj_remove_flag(rows[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(rows[i], LV_OBJ_FLAG_HIDDEN);
        row_style(rows[i], i == selected, visible && model_item_selectable(item));
        lv_obj_t *value = lv_obj_get_child(rows[i], 1);
        if(!value) continue;
        if(item->kind == MODEL_NAV || item->kind == MODEL_LOCKED ||
           item->kind == MODEL_ROOT_BACK) continue;
        uint8_t current = model_value(item);
        ui_settings_text_id_t text;
        if(item->option_count) {
            text = item->options[current % item->option_count];
        }
        else text = current ? UI_TEXT_ON : UI_TEXT_OFF;
        ui_settings_label_set_id(value, text, model_language());
        uint32_t color = !item->option_count && current ? COLOR_YELLOW : UI_COLOR_WHITE;
        if(lv_color_to_u32(lv_obj_get_style_text_color(value, 0)) !=
           lv_color_to_u32(lv_color_hex(color)))
            lv_obj_set_style_text_color(value, lv_color_hex(color), 0);
    }
}

static void model_step(ui_settings_state_t *state, const model_page_t *model, int step)
{
    uint8_t *index = model_index(state, model);
    uint8_t next = *index;
    if(next >= model_menu(model)->count) next = step > 0 ? (uint8_t)(model_menu(model)->count - 1U) : 0U;
    for(uint8_t i = 0; i < model_menu(model)->count; ++i) {
        next = (uint8_t)(((int)next + step + model_menu(model)->count) % model_menu(model)->count);
        if(model_item_selectable(model_item(model, next))) { *index = next; return; }
    }
    *index = NONE;
}

static bool model_key(ui_settings_page_t *page, uint8_t code)
{
    const model_page_t *model = model_find(page->st.view);
    if(page->st.view == UI_SETTINGS_VIEW_ROOT &&
       (!page->st.root_locked || page->st.root_index != UI_SETTINGS_ROOT_ITINERARY))
        model = NULL;
    if(!model) return false;
    uint8_t *index = model_index(&page->st, model);
    if(code == UI_KEY_UP || code == UI_KEY_DOWN) {
        model_step(&page->st, model, code == UI_KEY_UP ? -1 : 1);
        return true;
    }
    if(code != UI_KEY_OK) return false;
    if(*index >= model_menu(model)->count) { model_step(&page->st, model, 1); return true; }
    const model_item_t *item = model_item(model, *index);
    if(!model_item_selectable(item)) return true;
    if(item->kind == MODEL_ROOT_BACK) {
        page->st.root_locked = false;
        page->st.root_index = NONE;
        page->st.it_index = NONE;
        return true;
    }
    if(item->kind == MODEL_NAV) {
        model_reset_index(&page->st, item->target_view);
        page->st.view = item->target_view;
        return true;
    }
    if(item->kind == MODEL_BACK) { (void)ui_settings_page_back(page); return true; }
    uint8_t current = model_value(item);
    if(item->kind == MODEL_LOCAL) {
        if(s_settings) (void)item->set_local(s_settings, current == 0U);
    }
    else {
        uint8_t next = item->option_count ? (uint8_t)((current + 1U) % item->option_count)
                                         : (uint8_t)(current == 0U);
        vehicle_send(item->command_index, next);
    }
    return true;
}
