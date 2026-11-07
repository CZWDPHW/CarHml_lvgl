/*
 * page_model.h - PageModel 下各样式入口的声明
 */
#ifndef CARMW_PAGE_MODEL_H
#define CARMW_PAGE_MODEL_H

#include "ui/ui_node.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Style1：斯巴鲁森林人主表盘（对应 PageSubaruForester1.qml） */
extern const ui_node_vtbl_t page_subaru_forester1_vtbl;

/* GeneralUi1：占位样式，用于验证运行时切换 */
extern const ui_node_vtbl_t page_general1_vtbl;

#ifdef __cplusplus
}
#endif

#endif /* CARMW_PAGE_MODEL_H */
