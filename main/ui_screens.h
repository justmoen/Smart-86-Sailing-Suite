#pragma once
#include "lvgl.h"

#define LV_HOR_RES_MAX 720
#define LV_VER_RES_MAX 720

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lv_updatable_screen_t {
    lv_obj_t *screen;
    bool created;
    void (*create_cb)(struct lv_updatable_screen_t *);
    void (*update_cb)(struct lv_updatable_screen_t *);
    void *user_data;  // Optional context data (e.g., engine_id, tank_id, etc.)
} lv_updatable_screen_t;

#ifdef __cplusplus
}
#endif