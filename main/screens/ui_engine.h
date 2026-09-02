#ifndef UI_ENGINE_H
#define UI_ENGINE_H

#include "lvgl.h"
#include "ui_screens.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_updatable_screen_t engineScreen;

// Factory function to create N engine screens dynamically
void create_engine_screens(lv_updatable_screen_t **out_screens, int *out_count);
int get_engine_screen_count();

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*UI_ENGINE_H*/
