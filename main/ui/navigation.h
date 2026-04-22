#ifndef UI_NAVIGATION_H
#define UI_NAVIGATION_H

#include "lvgl.h"
#include "ui_screens.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int page;

void default_settings(void);
void gesture_event_cb(lv_event_t *e);
void navigation_next_page(void);
void navigation_prev_page(void);
void save_brightness(int value);
void save_last_screen(int index);
int load_last_screen();
void navigation_process_deferred_brightness(void);

#ifdef __cplusplus
}
#endif

#endif