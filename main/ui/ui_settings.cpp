#include "ui_settings.h"
#include "navigation.h"
#include <net_mdns.h>
#include <Preferences.h>
#include "keepalive.h"
#include "screen_config.h"

#ifdef __cplusplus
extern "C" {
#endif


static void lv_lcd_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider =
        lv_event_get_target_obj(e);

    int value =
        (int)lv_slider_get_value(slider);

    char buf[8];

    lv_snprintf(
        buf,
        sizeof(buf),
        "%d%%",
        value);

    lv_label_set_text(
        lcd_slider_label,
        buf);

    lv_obj_align_to(
        lcd_slider_label,
        slider,
        LV_ALIGN_OUT_BOTTOM_MID,
        0,
        10);

    save_brightness(value);
}


static void btnRotateScreen_event(lv_event_t *e)
{
    lv_event_code_t code =
        lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {

        preferences.begin(
            "scr-cfg",
            false);

        bool rotate =
            preferences.getBool(
                "ROTATE",
                false);

        if (rotate) {
            preferences.remove("ROTATE");
        }
        else {
            preferences.putBool(
                "ROTATE",
                true);
        }

        preferences.end();

        ESP_restart();
    }
}


void save_page(int page)
{
    preferences.begin(
        "scr-cfg",
        false);

    preferences.putInt(
        "PAGE",
        page);

    preferences.end();
}


static int restore_page()
{
    preferences.begin(
        "scr-cfg",
        false);

    int page =
        preferences.getInt(
            "PAGE",
            0);

    preferences.end();

    return page;
}


void lv_lcd_settings(lv_obj_t *parent)
{
    lcd_conf_obj =
        lv_obj_create(parent);

    lv_obj_center(
        lcd_conf_obj);

    lv_obj_set_size(
        lcd_conf_obj,
        300,
        220);


    // LCD brightness slider.
    lv_obj_t *slider =
        lv_slider_create(
            lcd_conf_obj);

    lv_obj_center(
        slider);

    lv_obj_add_event_cb(
        slider,
        lv_lcd_slider_event_cb,
        LV_EVENT_VALUE_CHANGED,
        NULL);


    // Brightness percentage label.
    lcd_slider_label =
        lv_label_create(
            lcd_conf_obj);

    lv_slider_set_value(
        slider,
        60,
        LV_ANIM_OFF);

    save_brightness(
        (int)lv_slider_get_value(slider));

    lv_label_set_text_static(
        lcd_slider_label,
        "60%");

    lv_obj_align_to(
        lcd_slider_label,
        slider,
        LV_ALIGN_OUT_BOTTOM_MID,
        0,
        10);


    // Hide the configuration panel initially.
    lv_obj_add_flag(
        lcd_conf_obj,
        LV_OBJ_FLAG_HIDDEN);


    // Screen rotation button.
    lv_obj_t *btn_rotate =
        lv_button_create(
            lcd_conf_obj);

    lv_obj_t *label_rotate =
        lv_label_create(
            btn_rotate);

    lv_obj_align(
        btn_rotate,
        LV_ALIGN_CENTER,
        0,
        60);

    lv_label_set_text_static(
        label_rotate,
        LV_SYMBOL_LOOP);

    lv_obj_center(
        label_rotate);

    lv_obj_add_event_cb(
        btn_rotate,
        btnRotateScreen_event,
        LV_EVENT_CLICKED,
        NULL);
}


static void edit_lcd_conf_evt_handler(lv_event_t *e)
{
    lv_event_code_t code =
        lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {

        lv_obj_remove_flag(
            lcd_conf_obj,
            LV_OBJ_FLAG_HIDDEN);
    }
}


#ifdef __cplusplus
} /* extern "C" */
#endif