#ifndef UI_WIND_H
#define UI_WIND_H

#include <lvgl.h>
#include <ui_screens.h>
#include "../ui/ui_init.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_updatable_screen_t windScreen;

void init_windScreen(void);
void lv_wind_display(lv_obj_t *parent);
void wind_update_cb(void);

#ifdef __cplusplus
}
#endif

#endif