#include "ui_settings_text.h"
#include <stddef.h>
#include "translations/en.h"
#include "translations/zh_cn.h"

/* 新增语言：include 对应资源表，再在这里登记原有语言编号。 */
static const char *const *const catalogs[19] = {
    [0] = text_zh_cn,
    [1] = text_en,
};

const char *ui_settings_text_get(ui_settings_text_id_t text, uint8_t language)
{
    if(text <= UI_TEXT_NONE || text >= UI_TEXT_COUNT) return "";
    const char *const *catalog = language < sizeof(catalogs) / sizeof(catalogs[0])
                               ? catalogs[language] : NULL;
    if(catalog && catalog[text] && catalog[text][0]) return catalog[text];
    return text_en[text] ? text_en[text] : "";
}
