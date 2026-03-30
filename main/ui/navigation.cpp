#pragma once

#include <HardwareSerial.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "navigation.h"
#include "ui_event_bus.h"
#include "ui_manager.h"
#include "bsp/display.h"
#include "nvs.h"

#define NVS_NAMESPACE "display"
#define NVS_KEY_BRIGHTNESS "brightness"

static lv_updatable_screen_t **nav_screens;

static int brightness = 50;

static lv_obj_t *brightness_overlay;
static lv_obj_t *brightness_bar;

static lv_timer_t *brightness_timer;

static int last_y = -1;
static bool brightness_active = false;


/* ---------------- NVS ---------------- */

void save_brightness(int value)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if(err == ESP_OK)
    {
        err = nvs_set_i32(handle, NVS_KEY_BRIGHTNESS, value);
        if(err != ESP_OK) { Serial.printf("NVS set failed: %d\n", err); }
        err = nvs_commit(handle);
        if(err != ESP_OK) { Serial.printf("NVS commit failed: %d\n", err); }
        nvs_close(handle);
    }
    else
    {
        Serial.printf("NVS open failed: %d\n", err);
    }
}

static int load_brightness()
{
    nvs_handle_t handle;
    int32_t value = 50;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if(err == ESP_OK)
    {
        nvs_get_i32(handle, NVS_KEY_BRIGHTNESS, &value);
        nvs_close(handle);
    }
    else
    {
        Serial.printf("NVS read failed: %d\n", err);
    }
    return value;
}


/* ---------------- Overlay ---------------- */

static void overlay_hide(lv_timer_t *t)
{
    LV_UNUSED(t);
    lv_obj_add_flag(brightness_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void overlay_create()
{
    if(brightness_overlay) return;

    brightness_overlay = lv_obj_create(lv_layer_top());

    lv_obj_set_size(brightness_overlay, 80, 260);
    lv_obj_align(brightness_overlay, LV_ALIGN_RIGHT_MID, -10, 0);

    lv_obj_set_style_bg_opa(brightness_overlay, LV_OPA_70, 0);

    brightness_bar = lv_bar_create(brightness_overlay);

    lv_obj_set_size(brightness_bar, 20, 200);
    lv_obj_center(brightness_bar);

    lv_bar_set_range(brightness_bar, 0, 100);

    lv_obj_add_flag(brightness_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void overlay_show()
{
    overlay_create();

    lv_obj_clear_flag(brightness_overlay, LV_OBJ_FLAG_HIDDEN);

    if(brightness_timer)
        lv_timer_del(brightness_timer);

    brightness_timer = lv_timer_create(overlay_hide, 3000, NULL);
}

/* ---------------- Gesture Handling ---------------- */

static bool right_edge(lv_point_t *p)
{
    int w = lv_disp_get_hor_res(NULL);
    return p->x > w * 0.85;
}

void gesture_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    lv_point_t p;
    lv_indev_get_point(lv_indev_get_act(), &p);

    if(code == LV_EVENT_PRESSED)
    {
        last_y = p.y;
        brightness_active = false;
    }

    if(code == LV_EVENT_PRESSING)
    {
        if(last_y < 0) return;

        if(!brightness_active && !right_edge(&p))
        {
            last_y = p.y;
            return;
        }

        int dy = p.y - last_y;

        if(abs(dy) > 8)
        {
            if(!brightness_active)
            {
                brightness_active = true;
                overlay_show();
            }

            brightness -= dy / 4;

            brightness = LV_CLAMP(0, brightness, 100);

            bsp_display_brightness_set(brightness);

            lv_bar_set_value(brightness_bar, brightness, LV_ANIM_OFF);

            ui_event_publish(UI_EVENT_BRIGHTNESS_CHANGED, &brightness);

            if(brightness_timer)
                lv_timer_reset(brightness_timer);
        }

        last_y = p.y;
    }

    if(code == LV_EVENT_RELEASED)
    {
        last_y = -1;
        save_brightness(brightness);  // save unconditionally
        brightness_active = false;
    }

    if(code == LV_EVENT_GESTURE)
    {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

        if(dir == LV_DIR_LEFT)
        {
            ui_manager_next();
        }

        if(dir == LV_DIR_RIGHT)
        {
            ui_manager_prev();
        }
    }
}

/* ---------------- Init ---------------- */

void default_settings()
{
    brightness = load_brightness();
    bsp_display_brightness_set(brightness);
}

#ifdef __cplusplus
}
#endif