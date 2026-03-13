#ifndef UI_NAVIGATION_H
#define UI_NAVIGATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "ui_screens.h"

void navigation_init(lv_updatable_screen_t **screens, int count);

void navigation_next_page(void);
void navigation_prev_page(void);

#ifdef __cplusplus
}
#endif

#endif