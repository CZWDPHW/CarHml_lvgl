/*
 * ui_style_registry.c - 样式注册表
 *
 * 新增样式只需：实现一个 PageModel 下的 .c，
 * 并在本文件加入一条描述项。
 */
#include "ui/ui_style.h"

#include <string.h>

#include "PageModel/page_model.h"

/*
 * vehicle 对应 Qt 的 currentCarId，available 表示该样式已迁移可上线。
 * general1 是 PublicUi 的 Style6 占位样式（仅用于验证运行时切换），
 * 不属于 Subaru Forester，因此不会出现在 Forester 的风格列表里。
 */
static const ui_style_desc_t s_styles[] = {
    { "forester1", "SubaruForester1", "subaru_forester", true,
      &page_subaru_forester1_vtbl },
    { "general1", "GeneralUi1", "public_ui", true, &page_general1_vtbl },
};

/* 返回注册表中的样式总数 */
size_t ui_style_count(void)
{
    return sizeof(s_styles) / sizeof(s_styles[0]);
}

/* 按序号取样式描述，用于“下一个样式”的循环遍历 */
const ui_style_desc_t *ui_style_at(size_t index)
{
    if(index >= ui_style_count()) {
        return NULL;
    }

    return &s_styles[index];
}

/* 按序号取样式标识，设置菜单列出可选项用 */
const char *ui_style_id_at(size_t index)
{
    if(index >= ui_style_count()) {
        return NULL;
    }

    return s_styles[index].id;
}

/* 按标识线性查找样式描述 */
const ui_style_desc_t *ui_style_find(const char *id)
{
    size_t count = ui_style_count();

    if(id == NULL) {
        return NULL;
    }

    for(size_t i = 0; i < count; i++) {
        if(strcmp(s_styles[i].id, id) == 0) {
            return &s_styles[i];
        }
    }

    return NULL;
}

/* 车型内可用样式数量（available 且 vehicle 相同） */
size_t ui_style_vehicle_count(const char *vehicle)
{
    size_t count = 0U;

    if(vehicle == NULL) {
        return 0U;
    }

    for(size_t i = 0; i < ui_style_count(); i++) {
        if(s_styles[i].available &&
           strcmp(s_styles[i].vehicle, vehicle) == 0) {
            ++count;
        }
    }

    return count;
}

/* 车型内第 index 个可用样式，越界返回 NULL */
const ui_style_desc_t *ui_style_vehicle_at(const char *vehicle, size_t index)
{
    size_t seen = 0U;

    if(vehicle == NULL) {
        return NULL;
    }

    for(size_t i = 0; i < ui_style_count(); i++) {
        if(s_styles[i].available &&
           strcmp(s_styles[i].vehicle, vehicle) == 0) {
            if(seen == index) {
                return &s_styles[i];
            }
            ++seen;
        }
    }

    return NULL;
}

/*
 * 同车型下一个可用样式（Qt 31 / PageMyCarStyle 的合法范围）。
 * 当前样式不在可用列表里时回到该车型第一个可用样式；
 * 该车型没有其它可用样式时返回 NULL，调用方保持原样式。
 */
const ui_style_desc_t *ui_style_next_for_vehicle(const ui_style_desc_t *current)
{
    size_t count;
    size_t index = 0U;
    bool found = false;

    if(current == NULL) {
        return NULL;
    }

    count = ui_style_vehicle_count(current->vehicle);
    if(count == 0U) {
        return NULL;
    }

    for(size_t i = 0; i < count; i++) {
        const ui_style_desc_t *desc = ui_style_vehicle_at(current->vehicle, i);

        if(desc == current) {
            index = i;
            found = true;
            break;
        }
    }

    if(!found) {
        return ui_style_vehicle_at(current->vehicle, 0U);
    }

    if(count <= 1U) {
        return NULL;
    }

    return ui_style_vehicle_at(current->vehicle, (index + 1U) % count);
}

/* 默认样式：注册表中第一个可用项（当前为 Forester Style1） */
const ui_style_desc_t *ui_style_default(void)
{
    for(size_t i = 0; i < ui_style_count(); i++) {
        if(s_styles[i].available) {
            return &s_styles[i];
        }
    }

    return NULL;
}
