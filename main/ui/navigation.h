#ifndef UI_NAVIGATION_H
#define UI_NAVIGATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "ui_screens.h"

extern int page;

void default_settings(void);
void gesture_event_cb(lv_event_t *e);
void navigation_next_page(void);
void navigation_prev_page(void);
void save_brightness(int value);

#ifdef __cplusplus
}
#endif

#endif