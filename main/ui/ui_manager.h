#pragma once

#include "ui_screens.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int current_index;

void ui_manager_init();
void ui_manager_update();

void ui_manager_show(lv_updatable_screen_t *screen);
void ui_manager_next();
void ui_manager_prev();

#ifdef __cplusplus
}
#endif