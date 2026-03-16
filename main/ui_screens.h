#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lv_updatable_screen_t {
    lv_obj_t *screen;
    bool created;
    void (*create_cb)(struct lv_updatable_screen_t *);
    void (*update_cb)(struct lv_updatable_screen_t *);
} lv_updatable_screen_t;

#ifdef __cplusplus
}
#endif