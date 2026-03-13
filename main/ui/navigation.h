#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    lv_obj_t *screen;
    void (*init_cb)(lv_obj_t *);
    void (*update_cb)(void);
} lv_updatable_screen_t;

void navigation_init(lv_updatable_screen_t **screens, int count);

void navigation_next_page(void);
void navigation_prev_page(void);

#ifdef __cplusplus
}
#endif