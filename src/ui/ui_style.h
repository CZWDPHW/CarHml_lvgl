/*
 * ui_style.h - 界面样式描述与注册表
 *
 * 一个样式对应 QML 的 PageModel/PageXXX.qml，实现为一个独立的 .c。
 */
#ifndef CARMW_UI_STYLE_H
#define CARMW_UI_STYLE_H

#include <stdbool.h>
#include <stddef.h>

#include "ui/ui_node.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *id;              /* 样式标识，如 "forester1" */
    const char *name;            /* 显示名称 */
    const char *vehicle;         /* Qt currentCarId 对应的车型标识 */
    bool available;              /* 该车型下已迁移并允许选择/切换 */
    const ui_node_vtbl_t *node;  /* 样式顶层组件 */
} ui_style_desc_t;

/* 注册表中已登记的样式数量 */
size_t ui_style_count(void);

/* 按序号取样式描述，index 越界返回 NULL */
const ui_style_desc_t *ui_style_at(size_t index);

/* 按样式标识查找，找不到或 id 为 NULL 时返回 NULL */
const ui_style_desc_t *ui_style_find(const char *id);

/* 按序号取样式标识，index 越界返回 NULL（PC 演示等全量遍历用） */
const char *ui_style_id_at(size_t index);

/*
 * 车型内可用样式（Qt PageMyCarStyle 按 currentCarId 生成列表）：
 * 只统计 available 且 vehicle 相同的条目，顺序与注册表一致。
 */
size_t ui_style_vehicle_count(const char *vehicle);
const ui_style_desc_t *ui_style_vehicle_at(const char *vehicle, size_t index);

/* 同车型下一个可用样式；没有其它可用样式时返回 NULL */
const ui_style_desc_t *ui_style_next_for_vehicle(const ui_style_desc_t *current);

/* 默认车型的默认样式（首个 available 条目） */
const ui_style_desc_t *ui_style_default(void);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_STYLE_H */
