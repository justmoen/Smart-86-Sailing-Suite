#ifndef UI_SCREENS_H
#define UI_SCREENS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lv_updatable_screen_t {
    lv_obj_t *screen;
    void (*init_cb)(lv_obj_t *parent);
    void (*update_cb)();
} lv_updatable_screen_t;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
