/*
 * ui_style_registry.c - 样式注册表
 *
 * 新增样式只需：实现一个 PageModel 下的 .c，
 * 并在本文件加入一条描述项。
 */
#include "ui/ui_style.h"

#include <string.h>

#include "PageModel/page_model.h"

static const ui_style_desc_t s_styles[] = {
    { "forester1", "SubaruForester1", &page_subaru_forester1_vtbl },
    { "general1", "GeneralUi1", &page_general1_vtbl },
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
