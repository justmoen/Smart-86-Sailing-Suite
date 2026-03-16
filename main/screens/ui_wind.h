#ifndef UI_WIND_H
#define UI_WIND_H

#include <lvgl.h>
#include <ui_screens.h>
#include "../ui/ui_init.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_updatable_screen_t windScreen;

static void lv_wind_display(lv_updatable_screen_t *scr);
static void wind_update_cb(lv_updatable_screen_t *scr);

#ifdef __cplusplus
}
#endif

#endif