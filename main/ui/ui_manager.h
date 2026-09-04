#pragma once

#include "ui_screens.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int current_index;

void ui_manager_init();
void ui_manager_update();
void ui_manager_process_deferred_screen_creation();
void ui_manager_process_deferred_screen_load();

void ui_manager_show(lv_updatable_screen_t *screen);
void ui_manager_next();
void ui_manager_prev();
void ui_manager_reinit_screens();

bool ui_manager_is_current_screen(lv_updatable_screen_t *screen);

struct EngineScreenState;

#ifdef __cplusplus
}
#endif