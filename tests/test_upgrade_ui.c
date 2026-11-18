#include "test_util.h"
#include "ui/overlays/ui_upgrade_popup.h"
#include "ui/ui_app.h"
#include "ui/car_ui_key.h"
#include "carmw/car_port_memory.h"
#include "carmw/car_protocol.h"

int g_checks,g_failures;
static uint32_t frame[1920*720];
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    (void)area; (void)pixels; lv_display_flush_ready(display);
}
static uint32_t next_utf8(const unsigned char **s)
{
    uint32_t c=*(*s)++;
    unsigned n=c<128?0:c<224?1:c<240?2:3;
    if(n) c&=(1U<<(6-n))-1;
    while(n--) c=(c<<6)|(*(*s)++&63);
    return c;
}
static void check_labels(lv_obj_t *object, const lv_area_t *bounds)
{
    if(lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN)) return;
    if(lv_obj_check_type(object,&lv_label_class)) {
        lv_area_t area; lv_obj_get_coords(object,&area);
        CHECK(area.x1>=bounds->x1 && area.x2<=bounds->x2);
        CHECK(area.y1>=bounds->y1 && area.y2<=bounds->y2);
        const lv_font_t *font=lv_obj_get_style_text_font(object,0);
        const unsigned char *s=(const unsigned char *)lv_label_get_text(object);
        while(*s) {
            uint32_t code=next_utf8(&s); if(code<33) continue;
            lv_font_glyph_dsc_t glyph={0};
            CHECK(lv_font_get_glyph_dsc(font,&glyph,code,0)); CHECK(!glyph.is_placeholder);
        }
    }
    for(unsigned i=0;i<lv_obj_get_child_count(object);++i) check_labels(lv_obj_get_child(object,(int32_t)i),bounds);
}
static void save(const char *prefix, const char *variant)
{
    char path[1024]; snprintf(path,sizeof(path),"%s-%s.ppm",prefix,variant);
    lv_refr_now(NULL); FILE *file=fopen(path,"wb"); CHECK(file); if(!file) return;
    fprintf(file,"P6\n1920 720\n255\n");
    for(unsigned i=0;i<1920U*720;++i) {
        uint8_t rgb[]={(uint8_t)(frame[i]>>16),(uint8_t)(frame[i]>>8),(uint8_t)frame[i]}; fwrite(rgb,1,3,file);
    }
    fclose(file);
}
int main(int argc, char **argv)
{
    lv_init(); lv_display_t *display=lv_display_create(1920,720); CHECK(display);
    lv_timer_pause(lv_display_get_refr_timer(display));
    lv_display_set_color_format(display,LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_flush_cb(display,flush);
    lv_display_set_buffers(display,frame,NULL,sizeof(frame),LV_DISPLAY_RENDER_MODE_DIRECT);
    CHECK(ui_app_init()); CHECK(ui_app_set_style("forester1"));
    car_settings_t settings; car_settings_reset(&settings); ui_app_settings_set_settings(&settings);
    car_state_view_t view={0}; view.has_data=true; ui_app_apply(&view);
    car_upgrade_status_t s={0}; s.visible=true; s.busy=true;
    s.phase[0]=CAR_UPGRADE_SUCCESS; s.phase[1]=CAR_UPGRADE_TRANSFERRING; s.phase[2]=CAR_UPGRADE_SUCCESS;
    s.mcu_percent=57;
    for(unsigned lang=0;lang<2;++lang) {
        ++s.revision; ui_upgrade_popup_update(&s,(uint8_t)lang,5);
        lv_obj_t *root=ui_upgrade_popup_root(); CHECK(root && ui_upgrade_popup_active());
        lv_obj_update_layout(root); lv_obj_t *dialog=lv_obj_get_child(root,0);
        CHECK(lv_obj_get_width(dialog)==800 && lv_obj_get_height(dialog)==580);
        lv_area_t bounds; lv_obj_get_coords(dialog,&bounds);
        CHECK(bounds.x1==560 && bounds.y1==70); check_labels(dialog,&bounds);
        if(argc>1) save(argv[1],lang?"progress-en":"progress-zh");
        /* Both direct UI keys and the special steering routes are blocked. */
        ui_app_settings_open(); CHECK(!ui_app_settings_is_open());
        car_port_t port; car_port_memory_t memory; car_link_t link;
        car_port_memory_attach(&port,&memory); CHECK(car_link_init(&link,&port));
        const uint8_t keys[]={13,19,20,21,22,23,26,31};
        for(unsigned k=0;k<sizeof(keys);++k) {
            uint8_t payload[]={keys[k],1},packet[8];
            size_t n=car_frame_encode(0x20,CAR_OP_STATUS,payload,2,packet,sizeof(packet));
            CHECK(n==7); car_port_memory_push(&memory,packet,n); car_link_pump(&link,0);
            CHECK(car_ui_key_pump(&link)==0); CHECK(!ui_app_settings_is_open());
        }
        car_link_deinit(&link);
        s.busy=false; s.finished=true; s.success=false; s.reboot_required=true;
        s.phase[1]=CAR_UPGRADE_FAILED; ++s.revision;
        ui_upgrade_popup_update(&s,(uint8_t)lang,5); lv_obj_update_layout(root); check_labels(dialog,&bounds);
        if(argc>1) save(argv[1],lang?"failure-en":"failure-zh");
        s.busy=true; s.finished=false; s.reboot_required=false; s.phase[1]=CAR_UPGRADE_TRANSFERRING;
    }
    s.visible=false; ui_upgrade_popup_update(&s,0,0); CHECK(!ui_upgrade_popup_active());
    ui_app_settings_open(); CHECK(ui_app_settings_is_open()); ui_app_settings_close();
    ui_app_deinit(); CHECK(ui_upgrade_popup_root()==NULL);
    lv_display_delete(display); lv_deinit();
    printf("Upgrade UI: %d checks, %d failures\n",g_checks,g_failures); return g_failures?1:0;
}
