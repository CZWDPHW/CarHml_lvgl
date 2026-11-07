/*
 * ui_style.h - 界面样式描述与注册表
 *
 * 一个样式对应 QML 的 PageModel/PageXXX.qml，实现为一个独立的 .c。
 */
#ifndef CARMW_UI_STYLE_H
#define CARMW_UI_STYLE_H

#include <stddef.h>

#include "ui/ui_node.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *id;              /* 样式标识，如 "forester1" */
    const char *name;            /* 显示名称 */
    const ui_node_vtbl_t *node;  /* 样式顶层组件 */
} ui_style_desc_t;

/* 注册表中已登记的样式数量 */
size_t ui_style_count(void);

/* 按序号取样式描述，index 越界返回 NULL */
const ui_style_desc_t *ui_style_at(size_t index);

/* 按样式标识查找，找不到或 id 为 NULL 时返回 NULL */
const ui_style_desc_t *ui_style_find(const char *id);

/* 按序号取样式标识，index 越界返回 NULL（设置菜单列出可选项用） */
const char *ui_style_id_at(size_t index);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_STYLE_H */
