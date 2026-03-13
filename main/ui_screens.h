#ifndef UI_SCREENS_H
#define UI_SCREENS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *screen;                  // root LVGL object for this screen
    void (*update_cb)(void);           // function to refresh dynamic content
} lv_updatable_screen_t;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
