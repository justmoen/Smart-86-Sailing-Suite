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
#define NVS_KEY_LAST_SCREEN "last_screen"

static lv_updatable_screen_t **nav_screens;

static int brightness = 50;

static lv_obj_t *brightness_overlay = nullptr;
static lv_obj_t *brightness_bar = nullptr;

static lv_timer_t *brightness_timer = nullptr;

static int last_y = -1;
static bool brightness_active = false;
static int pending_brightness = -1;


/*
 * One physical touch can generate multiple LV_EVENT_GESTURE events.
 *
 * This flag is now controlled by the INPUT DEVICE callbacks rather
 * than by the screen callbacks.
 *
 * false = this physical touch has not navigated yet
 * true  = this physical touch has already navigated
 */
static bool screen_gesture_handled = false;


/*
 * Once a horizontal navigation gesture has occurred, prevent the
 * remaining movement of that same physical touch from activating
 * the brightness control.
 */
static bool navigation_gesture_active = false;


/*
 * Prevent navigation_init() from registering duplicate callbacks.
 */
static bool navigation_input_initialized = false;


/* ---------------- NVS ---------------- */

void save_brightness(int value)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(
        NVS_NAMESPACE,
        NVS_READWRITE,
        &handle
    );

    if(err == ESP_OK)
    {
        err = nvs_set_i32(
            handle,
            NVS_KEY_BRIGHTNESS,
            value
        );

        if(err != ESP_OK)
        {
            Serial.printf(
                "NVS set failed: %d\n",
                err
            );
        }

        err = nvs_commit(handle);

        if(err != ESP_OK)
        {
            Serial.printf(
                "NVS commit failed: %d\n",
                err
            );
        }

        nvs_close(handle);
    }
    else
    {
        Serial.printf(
            "NVS open failed: %d\n",
            err
        );
    }
}


static int load_brightness()
{
    nvs_handle_t handle;
    int32_t value = 50;

    esp_err_t err =
        nvs_open(
            NVS_NAMESPACE,
            NVS_READONLY,
            &handle
        );

    if(err == ESP_OK)
    {
        nvs_get_i32(
            handle,
            NVS_KEY_BRIGHTNESS,
            &value
        );

        nvs_close(handle);
    }
    else
    {
        Serial.printf(
            "NVS read failed: %d\n",
            err
        );
    }

    return value;
}


void save_last_screen(int index)
{
    nvs_handle_t handle;

    if(nvs_open(
        NVS_NAMESPACE,
        NVS_READWRITE,
        &handle
    ) == ESP_OK)
    {
        nvs_set_i32(
            handle,
            NVS_KEY_LAST_SCREEN,
            index
        );

        nvs_commit(handle);

        nvs_close(handle);
    }
}


int load_last_screen()
{
    nvs_handle_t handle;
    int32_t value = 0;

    if(nvs_open(
        NVS_NAMESPACE,
        NVS_READONLY,
        &handle
    ) == ESP_OK)
    {
        nvs_get_i32(
            handle,
            NVS_KEY_LAST_SCREEN,
            &value
        );

        nvs_close(handle);
    }

    return value;
}


/* ---------------- Overlay ---------------- */

static void overlay_hide(lv_timer_t *t)
{
    LV_UNUSED(t);

    if(brightness_overlay)
    {
        lv_obj_add_flag(
            brightness_overlay,
            LV_OBJ_FLAG_HIDDEN
        );
    }
}


static void overlay_create()
{
    if(brightness_overlay)
        return;

    brightness_overlay =
        lv_obj_create(lv_layer_top());

    lv_obj_set_size(
        brightness_overlay,
        80,
        260
    );

    lv_obj_align(
        brightness_overlay,
        LV_ALIGN_RIGHT_MID,
        -10,
        0
    );

    lv_obj_set_style_bg_opa(
        brightness_overlay,
        LV_OPA_70,
        0
    );

    brightness_bar =
        lv_bar_create(
            brightness_overlay
        );

    lv_obj_set_size(
        brightness_bar,
        20,
        200
    );

    lv_obj_center(brightness_bar);

    lv_bar_set_range(
        brightness_bar,
        0,
        100
    );

    lv_bar_set_value(
        brightness_bar,
        brightness,
        LV_ANIM_OFF
    );

    lv_obj_add_flag(
        brightness_overlay,
        LV_OBJ_FLAG_HIDDEN
    );
}


static void overlay_show()
{
    overlay_create();

    lv_obj_remove_flag(
        brightness_overlay,
        LV_OBJ_FLAG_HIDDEN
    );

    if(brightness_timer)
    {
        lv_timer_del(
            brightness_timer
        );

        brightness_timer = nullptr;
    }

    brightness_timer =
        lv_timer_create(
            overlay_hide,
            3000,
            nullptr
        );
}


/* ---------------- Gesture Handling ---------------- */

static bool right_edge(lv_point_t *p)
{
    int w =
        lv_display_get_horizontal_resolution(NULL);

    return p->x > w * 0.85;
}


/* --------------------------------------------------------------------------
 * Input-device event callback
 *
 * This is the important part of the fix.
 *
 * These events belong to the persistent LVGL input device rather than
 * the currently displayed screen.
 *
 * Therefore a screen transition cannot cause us to lose RELEASED.
 * -------------------------------------------------------------------------- */

static void navigation_input_event_cb(lv_event_t *e)
{
    lv_event_code_t code =
        lv_event_get_code(e);

    lv_indev_t *indev =
        (lv_indev_t *)lv_event_get_current_target(e);

    if(!indev)
        return;


    /*
     * ---------------------------------------------------------
     * PHYSICAL TOUCH PRESSED
     * ---------------------------------------------------------
     */

    if(code == LV_EVENT_PRESSED)
    {
        lv_point_t p;

        lv_indev_get_point(
            indev,
            &p
        );

        /*
         * This is the ONLY place where a new physical touch
         * re-arms screen navigation.
         */
        screen_gesture_handled = false;

        navigation_gesture_active = false;

        last_y = p.y;

        brightness_active = false;

        return;
    }


    /*
     * ---------------------------------------------------------
     * PHYSICAL TOUCH RELEASED
     * ---------------------------------------------------------
     */

    if(code == LV_EVENT_RELEASED)
    {
        lv_point_t p;

        lv_indev_get_point(
            indev,
            &p
        );

        last_y = -1;

        /*
         * Preserve the existing behavior of saving brightness
         * when the physical touch ends.
         */
        save_brightness(
            brightness
        );

        brightness_active = false;

        navigation_gesture_active = false;

        /*
         * Re-arm for the NEXT physical touch.
         */
        screen_gesture_handled = false;

        return;
    }
}


/* --------------------------------------------------------------------------
 * Initialize input-device callbacks
 * -------------------------------------------------------------------------- */

void navigation_init()
{
    if(navigation_input_initialized)
        return;

    navigation_input_initialized = true;

    int pointer_count = 0;

    /*
     * LVGL maintains a global list of input devices.
     *
     * Register our lifecycle callbacks on every pointer device.
     */
    lv_indev_t *indev =
        lv_indev_get_next(NULL);

    while(indev)
    {
        lv_indev_type_t type =
            lv_indev_get_type(indev);

        if(type == LV_INDEV_TYPE_POINTER)
        {

            lv_indev_add_event_cb(
                indev,
                navigation_input_event_cb,
                LV_EVENT_PRESSED,
                nullptr
            );

            lv_indev_add_event_cb(
                indev,
                navigation_input_event_cb,
                LV_EVENT_RELEASED,
                nullptr
            );

            pointer_count++;
        }

        indev =
            lv_indev_get_next(indev);
    }
}


/* --------------------------------------------------------------------------
 * Screen event callback
 *
 * IMPORTANT:
 *
 * There is intentionally NO LV_EVENT_PRESSED or LV_EVENT_RELEASED
 * handling here.
 *
 * Those events are handled by navigation_input_event_cb(), which is
 * attached directly to the persistent input device.
 * -------------------------------------------------------------------------- */

void gesture_event_cb(lv_event_t *e)
{
    lv_event_code_t code =
        lv_event_get_code(e);

    lv_indev_t *indev =
        lv_indev_get_act();

    lv_point_t p;

    if(indev)
    {
        lv_indev_get_point(
            indev,
            &p
        );
    }
    else
    {
        p.x = 0;
        p.y = 0;
    }


    /*
     * ---------------------------------------------------------
     * BRIGHTNESS ADJUSTMENT
     * ---------------------------------------------------------
     */

    if(code == LV_EVENT_PRESSING)
    {
        if(!indev)
            return;

        /*
         * A horizontal navigation gesture has already consumed
         * this physical touch.
         */
        if(navigation_gesture_active)
            return;

        if(last_y < 0)
            return;


        /*
         * Until the touch moves vertically while inside the
         * right-edge area, do not activate brightness control.
         */
        if(
            !brightness_active &&
            !right_edge(&p)
        )
        {
            last_y = p.y;

            return;
        }


        int dy =
            p.y - last_y;


        if(abs(dy) > 8)
        {
            if(!brightness_active)
            {
                brightness_active = true;

                overlay_show();
            }


            /*
             * Moving upward increases brightness.
             * Moving downward decreases brightness.
             */
            brightness -=
                dy / 4;


            brightness =
                LV_CLAMP(
                    0,
                    brightness,
                    100
                );


            pending_brightness =
                brightness;


            if(brightness_bar)
            {
                lv_bar_set_value(
                    brightness_bar,
                    brightness,
                    LV_ANIM_OFF
                );
            }


            ui_event_publish(
                UI_EVENT_BRIGHTNESS_CHANGED,
                &brightness
            );


            if(brightness_timer)
            {
                lv_timer_reset(
                    brightness_timer
                );
            }
        }


        last_y =
            p.y;

        return;
    }


    /*
     * ---------------------------------------------------------
     * SCREEN NAVIGATION
     * ---------------------------------------------------------
     */

    if(code == LV_EVENT_GESTURE)
    {
        if(!indev)
            return;


        /*
         * Brightness adjustment owns this touch.
         */
        if(brightness_active)
            return;


        /*
         * Exactly one screen transition per physical touch.
         *
         * This is reset only by the input-device PRESSED/RELEASED
         * callbacks.
         */
        if(screen_gesture_handled)
        {
            return;
        }


        lv_dir_t dir =
            lv_indev_get_gesture_dir(indev);

        if(dir == LV_DIR_LEFT)
        {
            /*
             * Set the guard BEFORE changing screens.
             */
            screen_gesture_handled = true;

            navigation_gesture_active = true;


            ui_manager_next();

            return;
        }


        if(dir == LV_DIR_RIGHT)
        {
            /*
             * Set the guard BEFORE changing screens.
             */
            screen_gesture_handled = true;

            navigation_gesture_active = true;


            ui_manager_prev();

            return;
        }
    }
}


/* ---------------- Init ---------------- */

void default_settings()
{
    brightness =
        load_brightness();

    brightness =
        LV_CLAMP(
            0,
            brightness,
            100
        );

    bsp_display_brightness_set(
        brightness
    );
}


/*
 * Process deferred brightness changes.
 *
 * Called from the main loop outside the display/LVGL lock.
 */
void navigation_process_deferred_brightness()
{
    if(pending_brightness >= 0)
    {
        bsp_display_brightness_set(
            pending_brightness
        );

        pending_brightness = -1;
    }
}


#ifdef __cplusplus
}
#endif