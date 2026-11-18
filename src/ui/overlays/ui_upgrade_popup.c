#include "ui/overlays/ui_upgrade_popup.h"
#include "ui/settings/ui_settings_text.h"
#include "ui/ui_fonts.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *root,*dialog,*title,*section[3],*dot[3],*tips[3],*heading[3],*bar,*footer,*status_label,*error_label;
static unsigned last_revision=~0U,last_seconds=~0U;
static uint8_t last_language=255;
static car_upgrade_phase_t last_phase[3];
static bool restart_failed;
static const char *text(ui_settings_text_id_t id, uint8_t language) { return ui_settings_text_get(id,language); }
static void clean(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj); lv_obj_remove_flag(obj,LV_OBJ_FLAG_SCROLLABLE);
}
static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int width)
{
    lv_obj_t *obj=lv_label_create(parent);
    lv_obj_set_style_text_font(obj,font,0); lv_obj_set_style_text_color(obj,lv_color_hex(color),0);
    lv_obj_set_style_text_align(obj,LV_TEXT_ALIGN_CENTER,0);
    if(width) lv_obj_set_width(obj,width);
    lv_label_set_text(obj,""); return obj;
}
static void opacity(void *obj, int32_t value) { lv_obj_set_style_opa(obj,(lv_opa_t)value,0); }
static void create(void)
{
    root=lv_obj_create(lv_layer_top()); clean(root);
    lv_obj_set_size(root,LV_PCT(100),LV_PCT(100)); lv_obj_add_flag(root,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(root,lv_color_black(),0); lv_obj_set_style_bg_opa(root,LV_OPA_50,0);
    dialog=lv_obj_create(root); clean(dialog); lv_obj_set_size(dialog,800,580); lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog,lv_color_hex(0x1a252f),0); lv_obj_set_style_bg_opa(dialog,255,0);
    lv_obj_set_style_border_color(dialog,lv_color_hex(0x34495e),0); lv_obj_set_style_border_width(dialog,2,0);
    lv_obj_set_style_radius(dialog,12,0); lv_obj_set_style_pad_all(dialog,23,0);
    lv_obj_set_flex_flow(dialog,LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(dialog,15,0);
    title=label(dialog,UI_TEXT_BOLD(32),0xecf0f1,750);
    static const unsigned order[]={CAR_UPGRADE_APP,CAR_UPGRADE_MCU,CAR_UPGRADE_SYSTEM};
    for(unsigned row=0;row<3;++row) {
        unsigned i=order[row]; section[i]=lv_obj_create(dialog); clean(section[i]);
        lv_obj_set_size(section[i],750,LV_SIZE_CONTENT); lv_obj_set_style_radius(section[i],8,0);
        lv_obj_set_style_bg_opa(section[i],255,0); lv_obj_set_style_pad_ver(section[i],15,0);
        lv_obj_set_style_pad_hor(section[i],20,0); lv_obj_set_style_pad_row(section[i],8,0);
        lv_obj_set_flex_flow(section[i],LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(section[i],LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
        lv_obj_t *line=lv_obj_create(section[i]); clean(line);
        lv_obj_set_size(line,710,LV_SIZE_CONTENT); lv_obj_set_flex_flow(line,LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(line,15,0);
        lv_obj_set_flex_align(line,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
        heading[i]=label(line,UI_TEXT_BOLD(18),0xffffff,0);
        dot[i]=lv_obj_create(line); clean(dot[i]); lv_obj_set_size(dot[i],14,14);
        lv_obj_set_style_radius(dot[i],7,0); lv_obj_set_style_bg_opa(dot[i],255,0);
        if(i==CAR_UPGRADE_MCU) {
            bar=lv_bar_create(section[i]); lv_obj_set_size(bar,568,17);
            lv_obj_set_style_bg_color(bar,lv_color_hex(0x1a3a5c),LV_PART_MAIN);
            lv_obj_set_style_bg_color(bar,lv_color_hex(0x00d4ff),LV_PART_INDICATOR);
            lv_obj_set_style_radius(bar,3,LV_PART_MAIN); lv_obj_set_style_radius(bar,3,LV_PART_INDICATOR);
            lv_obj_add_flag(bar,LV_OBJ_FLAG_HIDDEN);
        }
        tips[i]=label(section[i],UI_TEXT(14),0xbdc3c7,710);
        last_phase[i]=(car_upgrade_phase_t)-1;
    }
    footer=lv_obj_create(dialog); clean(footer); lv_obj_set_size(footer,750,90);
    lv_obj_set_style_bg_opa(footer,255,0); lv_obj_set_style_bg_color(footer,lv_color_hex(0x2c3e50),0);
    lv_obj_set_style_radius(footer,8,0); lv_obj_set_style_border_width(footer,1,0);
    lv_obj_t *column=lv_obj_create(footer); clean(column); lv_obj_set_size(column,730,LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(column,LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(column,5,0);
    lv_obj_center(column);
    status_label=label(column,UI_TEXT_BOLD(18),0xffffff,730);
    error_label=label(column,UI_TEXT(15),0xf1c40f,730);
}
static bool active(car_upgrade_phase_t p)
{
    return p>=CAR_UPGRADE_CHECKING && p<=CAR_UPGRADE_TRANSFERRING;
}
void ui_upgrade_popup_update(const car_upgrade_status_t *s, uint8_t language, unsigned seconds)
{
    if(!s || !s->visible) {
        if(root) lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if(!root) create();
    if(lv_obj_has_flag(root,LV_OBJ_FLAG_HIDDEN)) { lv_obj_remove_flag(root,LV_OBJ_FLAG_HIDDEN); last_revision=~0U; }
    if(last_revision==s->revision && last_language==language && last_seconds==seconds) return;
    last_revision=s->revision; last_language=language; last_seconds=seconds;
    if(s->busy) restart_failed=false;
    lv_label_set_text(title,text(UI_TEXT_UPGRADE_TITLE,language));
    static const ui_settings_text_id_t titles[]={UI_TEXT_UPGRADE_APP,UI_TEXT_UPGRADE_MCU,UI_TEXT_UPGRADE_SYSTEM};
    static const ui_settings_text_id_t phases[]={UI_TEXT_UPGRADE_WAITING,UI_TEXT_UPGRADE_WAITING,
        UI_TEXT_UPGRADE_CHECKING,UI_TEXT_UPGRADE_COPYING,UI_TEXT_UPGRADE_PREPARING,
        UI_TEXT_UPGRADE_TRANSFERRING,UI_TEXT_UPGRADE_SUCCESS,UI_TEXT_UPGRADE_CURRENT,
        UI_TEXT_UPGRADE_MISSING,UI_TEXT_UPGRADE_FAILED,UI_TEXT_UPGRADE_INTERRUPTED,UI_TEXT_UPGRADE_SAVE_FAILED};
    for(unsigned i=0;i<3;++i) {
        car_upgrade_phase_t p=s->phase[i];
        bool in_progress=active(p),success=p==CAR_UPGRADE_SUCCESS || p==CAR_UPGRADE_CURRENT;
        lv_label_set_text(heading[i],text(titles[i],language));
        lv_obj_set_style_bg_color(section[i],lv_color_hex(in_progress?0x34495e:0x2c3e50),0);
        lv_obj_set_style_bg_color(dot[i],lv_color_hex(success?0x27ae60:in_progress?0xf39c12:0x7f8c8d),0);
        if(last_phase[i]!=p) {
            lv_anim_delete(dot[i],opacity); lv_obj_set_style_opa(dot[i],255,0);
            if(in_progress) {
                lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a,dot[i]); lv_anim_set_exec_cb(&a,opacity);
                lv_anim_set_values(&a,255,77); lv_anim_set_duration(&a,800);
                lv_anim_set_playback_duration(&a,800); lv_anim_set_repeat_count(&a,LV_ANIM_REPEAT_INFINITE); lv_anim_start(&a);
            }
            last_phase[i]=p;
        }
        if(i==CAR_UPGRADE_MCU && in_progress) {
            lv_obj_remove_flag(bar,LV_OBJ_FLAG_HIDDEN); lv_bar_set_value(bar,(int32_t)s->mcu_percent,LV_ANIM_OFF);
            lv_label_set_text_fmt(tips[i],"%u%%",s->mcu_percent);
        } else {
            if(i==CAR_UPGRADE_MCU) lv_obj_add_flag(bar,LV_OBJ_FLAG_HIDDEN);
            ui_settings_text_id_t id=(unsigned)p<sizeof(phases)/sizeof(phases[0])?phases[p]:UI_TEXT_UPGRADE_FAILED;
            if(i==CAR_UPGRADE_APP) {
                if(p==CAR_UPGRADE_SUCCESS) id=UI_TEXT_UPGRADE_APP_SUCCESS;
                if(p==CAR_UPGRADE_CURRENT) id=UI_TEXT_UPGRADE_APP_CURRENT;
                if(p==CAR_UPGRADE_MISSING) id=UI_TEXT_UPGRADE_APP_MISSING;
                if(p==CAR_UPGRADE_COPYING) id=UI_TEXT_UPGRADE_APP_COPY;
            } else if(i==CAR_UPGRADE_MCU) {
                if(p==CAR_UPGRADE_SUCCESS) id=UI_TEXT_UPGRADE_MCU_SUCCESS;
                if(p==CAR_UPGRADE_CURRENT) id=UI_TEXT_UPGRADE_MCU_CURRENT;
                if(p==CAR_UPGRADE_MISSING) id=UI_TEXT_UPGRADE_MCU_MISSING;
                if(p==CAR_UPGRADE_COPYING) id=UI_TEXT_UPGRADE_MCU_COPY;
            } else {
                if(p==CAR_UPGRADE_SUCCESS) id=UI_TEXT_UPGRADE_SYSTEM_SUCCESS;
                if(p==CAR_UPGRADE_COPYING) id=UI_TEXT_UPGRADE_SYSTEM_COPY;
                if(p==CAR_UPGRADE_FAILED) id=UI_TEXT_UPGRADE_SYSTEM_FAILED;
            }
            lv_label_set_text(tips[i],text(id,language));
        }
    }
    char message[512],countdown[150]="";
    if(s->finished && s->reboot_required && !restart_failed)
        snprintf(countdown,sizeof(countdown),text(UI_TEXT_UPGRADE_REBOOT,language),seconds);
    ui_settings_text_id_t id=s->finished?(s->success?UI_TEXT_UPGRADE_ALL_SUCCESS:UI_TEXT_UPGRADE_ALL_FAILED):
        s->interrupted_mcu && !s->busy?UI_TEXT_UPGRADE_INTERRUPTED:UI_TEXT_UPGRADE_INIT;
    snprintf(message,sizeof(message),"%s%s%s",text(id,language),countdown[0]?"\n":"",countdown);
    lv_label_set_text(status_label,message);
    const char *error=restart_failed?text(UI_TEXT_UPGRADE_REBOOT_FAILED,language):
        s->serial_failed?text(UI_TEXT_UPGRADE_SERIAL_FAILED,language):
        s->finished && !s->success?text(UI_TEXT_UPGRADE_RETRY,language):"";
    lv_label_set_text(error_label,error);
    if(error[0]) lv_obj_remove_flag(error_label,LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(error_label,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_border_color(footer,lv_color_hex(error[0]?0xe74c3c:0x3498db),0);
}
void ui_upgrade_popup_destroy(void)
{
    if(root) lv_obj_delete(root);
    root=NULL; restart_failed=false; last_revision=last_seconds=~0U; last_language=255;
}
bool ui_upgrade_popup_active(void) { return root && !lv_obj_has_flag(root,LV_OBJ_FLAG_HIDDEN); }
void ui_upgrade_popup_restart_failed(void) { restart_failed=true; last_revision=~0U; }
lv_obj_t *ui_upgrade_popup_root(void) { return root; }
