#include "ui_settings_language.h"
#include "lvgl.h"
#include "ui/ui_fonts.h"
#include <string.h>
#include <stdio.h>
static const lv_font_t *cjk_font_for(const lv_font_t *original)
{
    return ui_font_cjk(original);
}

static const char *const words[][2] = {
 {"Chinese","简体中文"},{"English","英语"},{"Traditional Chinese","繁体中文"},{"Russian","俄语"},{"Spanish","西班牙语"},{"German","德语"},{"Turkish","土耳其语"},{"Korean","韩语"},{"French","法语"},{"Italian","意大利语"},{"Portuguese","葡萄牙语"},{"Bulgarian","保加利亚语"},{"Polish","波兰语"},{"Romanian","罗马尼亚语"},{"Dutch","荷兰语"},{"Czech","捷克语"},{"Japanese","日语"},{"Greek","希腊语"},{"Arabic","阿拉伯语"},
 {"YEAR","年"},{"MONTH","月"},{"DAY","日"},{"HOUR","时"},{"MINUTE","分"},{"SEND DATE & TIME","发送日期时间"},{"FORMAT","格式"},{"SOURCE","来源"},{"INSTRUMENT","仪表"},{"CENTRAL","中控"},
 {"CONFIG ONLY","仅保存配置"},{"DEFAULT","默认"},{"ANDROID AUTO","安卓车机互联"},{"ANIMATION 1","动画一"},{"PLAYER UNAVAILABLE","播放器不可用"},{"SDK UNAVAILABLE","连接组件不可用"},{"ENABLE","启用"},{"UNAVAILABLE","不可用"},{"NO MODEL DATA","无车型数据"},
 {"FAILED / PARTIAL CLEANUP","清理失败或未全部完成"},{"LOG DIRECTORY CONTENTS ONLY","仅清理日志目录内容"},{"CONFIG / RESOURCES / MD5 KEPT","保留配置资源及校验文件"},
 {"MILEAGE","里程"},{"TEMP","温度"},{"MILE","英里"},{"THROTTLE","油门"},{"CUR","当前"},{"BEST","最佳"},{"ECO","节能"},
 {"TIME SETTINGS","时间设置"},{"TIME SOURCE: CENTRAL","时间源: 中控"},{"TIME SOURCE: INSTRUMENT","时间源: 仪表"},{"TIME SOURCE: UNKNOWN","时间源: 未知"},
 {"TIME FORMAT: 24h","时间制式: 24小时"},{"TIME FORMAT: 12h","时间制式: 12小时"},{"TIME FORMAT: ---","时间制式: ---"},{"OPERATE ON AUDIO UNIT","请在音响里操作"},
 {"WELCOME LIGHT ","迎宾灯 "},{"AUTO INTERIOR LIGHT ","室内自动点灯 "},{"LIGHT OFF DELAY","灯光关闭延时"},
 {"HORN FEEDBACK ","喇叭响应 "},{"AUTO UNLOCK ","自动解锁 "},{"EXTERNAL DOORS ","外部车门 "},{"SMART KEY LOCK ","智能钥匙上锁 "},{"AUTO DOOR LOCK","车门自动上锁"},
 {"IGN OFF","点火关闭"},{"P SHIFT","驻车挡"},{"SLOW","慢"},{"STANDARD","标准"},{"VERY FAST","很快"},{"FAST","快"},{"SPEED","车速"},
 {"LANE CHANGE FLASH ","变道闪灯 "},{"SPEED SENSING WIPER ","感应雨刷 "},{"REAR WIPER LINKAGE ","后雨刷联动 "},{"MIST MODE ","除雾模式 "},
 {"OIL CHANGE","更换机油"},{"TIRE CHANGE","更换轮胎"},{"CUSTOM ITEM","自定义项目"},{"SET SERVICE KM","设置保养里程"},{"REMAINING KM","剩余里程"},{"RESET","重置"},
 {"GENERAL SETTINGS","常规设置"},{"ECO SETTINGS","节能设置"},{"DRIVER ASSIST","驾驶员辅助"},{"SPEED WARNING","超速预警"},{"VEHICLE SETTINGS","车辆设置"},{"DOOR SETTINGS","车门设置"},{"MAINTENANCE","保养"},{"LIGHT SENSITIVITY","点灯灵敏度"},
 {"APP VERSION","应用版本"},{"MAIN FIRMWARE","主固件"},{"SMALL CHIP","小芯片"},{"WIRED","有线模块"},{"SYSTEM VERSION","系统版本"},{"READ FAILED","读取失败"},{"UNKNOWN","未知"},{"CHANGES SAVED ON ACC OFF","熄火后保存更改"},
 {"FACTORY SETTINGS","工厂设置"},{"SYSTEM SETTINGS","系统设置"},{"LANGUAGE","语言"},
 {"DATE & TIME","日期时间"},{"VERSION INFO","版本信息"},{"CLEAR CACHE","清缓存"},{"BOOT ANIMATION","开机动画"},
 {"ITINERARY","行程资讯"},{"INFORMATION","信息选项"},{"MY CAR","我的座驾"},{"SYSTEM","系统"},{"CONNECT","互联"},
 {"DIAL STYLE","仪表风格"},{"UNITS","计量单位"},{"CENTER CARDS","中间卡片"},{"GENERAL","常规设置"},{"ECO HISTORY","节能历史"},
 {"DRIVING INFO","行车信息"},{"OUTSIDE TEMP ","车外温度 "},{"GEAR ","档位显示 "},{"AVG FUEL ","平均油耗 "},
 {"DAY LIGHT ","白天亮度 "},{"NIGHT LIGHT ","夜间亮度 "},{"SPEED LIMIT ","超速报警 "},{"SPEED WARN ","超速预警 "},
 {"ECO DISPLAY ","节能显示 "},{"ECO INDICATOR ","节能指示 "},{"DRIVING ASSIST","驾驶辅助"},{"PARKING ASSIST","驻车辅助"},{"CHASSIS CONTROL","底盘控制"},
 {"EMERGENCY BRAKING","紧急制动"},{"LANE DEPARTURE ","车道偏离 "},{"BLIND SPOT ","变道盲区 "},
 {"LIGHT SETTINGS","灯光设置"},{"TURN LIGHT","转向灯"},{"DOORS","车门设置"},{"WIPER","雨刷器"},{"SERVICE","保养设置"},{"VEHICLE","车辆设置"},
 {"REMOTE ENGINE START","远程启动发动机"},{"SMART DOOR LOCK","智能钥匙上锁"},{"SELECT UNLOCK","选择解锁"},{"EXTERNAL DOOR","外部车门开关"},{"SINGLE DOOR UNLOCK","单车门解锁"},
 {"HEADLIGHT SENSITIVITY","点灯灵敏度"},{"HEADLIGHT OFF DELAY","灯光关闭延时"},{"ELECTRIC POWER STEERING","电动助力转向"},{"REAR CROSS TRAFFIC ALERT","倒车侧预警"},{"AUTO STOP","自动启停"},
 {"TOTAL MILEAGE","总里程"},{"FUEL TANK","油箱"},{"DATA DEBUG: UNAVAILABLE","数据调试：不可用"},{"TIRE DISPLAY","胎压显示"},{"CAR MODEL","车型设置"},{"FUNCTION SWITCHES","功能开关"},
 {"DOOR DISPLAY","车门状态"},{"RADAR","雷达状态"},{"4WD DISPLAY","四驱信息"},{"FUEL FILLER RIGHT","加油口右侧"},{"RIGHT HAND DRIVE","右舵模式"},{"LOGO: UNAVAILABLE","开机标志：不可用"},
 {"PASSWORD","密码"},{"WRONG PASSWORD","密码错误"},{"PLATFORM UNAVAILABLE","平台不可用"},{"CONFIRM / BACK","确认并返回"},{"CANCEL / BACK","取消并返回"},{"CONFIRM","确认"},{"CANCEL","取消"},{"BACK","返回"},
 {"UNSUPPORTED","暂不支持"},{"ON","开"},{"OFF","关"},{"CLEAR LOGS","清理日志"},{"CONFIRM CLEAR LOGS","确认清理日志"},{"LOGS CLEARED","日志已清理"},{"CLEANING LOGS...","正在清理日志"},
 {"NAV","导航"},{"TIRE","胎压"},{"CHASSIS","底盘"},{"SHORT TERM","短期行程"},{"FUEL","油耗"},{"MUSIC","音乐"}
};
typedef struct {
    char original[512];
    char translated[512];
    char displayed[512];
    bool ready;
    bool chinese;
    const lv_font_t *font;
} label_translation_t;

/* 逐字节跳过 UTF-8 序列，返回码点数 */
static size_t utf8_skip(const char *text)
{
    unsigned char lead = (unsigned char)*text;

    if(lead < 0x80U) return 1U;
    if((lead & 0xE0U) == 0xC0U) return 2U;
    if((lead & 0xF0U) == 0xE0U) return 3U;
    return 4U;
}

/* 子集字库缺字检查：缺字时该字符会显示成占位方块，等同布局溢出 */
static bool cjk_covers(const char *text, const lv_font_t *font)
{
    while(*text) {
        size_t len = utf8_skip(text);
        uint32_t cp;
        lv_font_glyph_dsc_t dsc;

        if(len == 1U) {
            cp = (unsigned char)text[0];
        }
        else {
            static const uint32_t mask[] = {0U, 0U, 0x1FU, 0x0FU, 0x07U};
            cp = (uint32_t)((unsigned char)text[0] & mask[len]);
            for(size_t i = 1U; i < len; ++i) {
                cp = (cp << 6) | ((unsigned char)text[i] & 0x3FU);
            }
        }
        if(cp >= 0x80U &&
           !lv_font_get_glyph_dsc(font, &dsc, cp, 0U)) {
            return false;
        }
        text += len;
    }
    return true;
}

static void translation_delete(lv_event_t *event)
{
    lv_free(lv_event_get_user_data(event));
}

void ui_settings_label_set_text(lv_obj_t *label, const char *text)
{
    if(text) {
        for(uint32_t i = 0; i < lv_obj_get_event_count(label); ++i) {
            lv_event_dsc_t *dsc = lv_obj_get_event_dsc(label, i);
            if(lv_event_dsc_get_cb(dsc) == translation_delete) {
                label_translation_t *saved = lv_event_dsc_get_user_data(dsc);
                if(saved->ready && strcmp(text, saved->original) == 0) return;
                saved->ready = false;
                break;
            }
        }
        if(strcmp(lv_label_get_text(label), text) == 0) return;
    }
    lv_label_set_text(label, text);
}

void ui_settings_translate(lv_obj_t *root, bool chinese)
{
    if(!root) return;
    /* Hidden branches are translated when they are next shown. */
    if(lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) return;
    if(lv_obj_check_type(root, &lv_label_class)) {
        label_translation_t *saved = NULL;
        for(uint32_t i = 0; i < lv_obj_get_event_count(root); ++i) {
            lv_event_dsc_t *dsc = lv_obj_get_event_dsc(root, i);
            if(lv_event_dsc_get_cb(dsc) == translation_delete)
                saved = lv_event_dsc_get_user_data(dsc);
        }
        if(!saved) {
            saved = lv_malloc_zeroed(sizeof(*saved));
            if(!saved) return;
            saved->font = lv_obj_get_style_text_font(root, 0);
            lv_obj_add_event_cb(root, translation_delete, LV_EVENT_DELETE, saved);
        }
        if(saved->ready && saved->chinese == chinese &&
           strcmp(lv_label_get_text(root), saved->displayed) == 0) return;
        /* LVGL DOTS 临时改写字符串；恢复原文后再缓存，避免把省略号当译文源。 */
        if(lv_label_get_long_mode(root) == LV_LABEL_LONG_MODE_DOTS)
            lv_label_set_text(root, NULL);
        const char *text = lv_label_get_text(root);
        if(strcmp(text, saved->translated) != 0)
            snprintf(saved->original, sizeof(saved->original), "%s", text);
        /*
         * 标签文本是运行时拼出来的（如 "时间源: 中控"）。词库缺词时回退原文，
         * 中文子集字库缺字时回退英文，避免出现方块字撑破行宽。
         */
        if(chinese) {
            const char *source = saved->original;
            size_t used = 0;
            while(*source && used + 1 < sizeof(saved->translated)) {
                const char *replacement = NULL;
                size_t longest = 0;
                for(unsigned i = 0; i < sizeof(words)/sizeof(words[0]); ++i) {
                    size_t n = strlen(words[i][0]);
                    if(n > longest && strncmp(source, words[i][0], n) == 0 &&
                       (source[n] == '\0' || source[n] == ' ' || source[n] == ':' || source[n] == ')' ||
                        words[i][0][n - 1] == ' ')) {
                        longest = n;
                        replacement = words[i][1];
                    }
                }
                if(replacement) {
                    size_t n = strlen(replacement);
                    if(used + n >= sizeof(saved->translated)) break;
                    memcpy(saved->translated + used, replacement, n);
                    used += n;
                    source += longest;
                }
                else saved->translated[used++] = *source++;
            }
            saved->translated[used] = '\0';
            if(cjk_covers(saved->translated, cjk_font_for(saved->font))) {
                lv_label_set_text(root, saved->translated);
                /* The selected subset contains Latin, symbols and CJK at the same pixel size. */
                lv_obj_set_style_text_font(root, cjk_font_for(saved->font), 0);
            }
            else {
                lv_label_set_text(root, saved->original);
                lv_obj_set_style_text_font(root, saved->font, 0);
            }
        }
        else {
            lv_label_set_text(root, saved->original);
            lv_obj_set_style_text_font(root, saved->font, 0);
        }
        saved->ready = true;
        saved->chinese = chinese;
        snprintf(saved->displayed, sizeof(saved->displayed), "%s", lv_label_get_text(root));
    }
    for(uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
        ui_settings_translate(lv_obj_get_child(root, i), chinese);
}
