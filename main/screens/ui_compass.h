#ifndef UI_COMPASS_H
#define UI_COMPASS_H

#include "ui_screens.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_updatable_screen_t compassScreen;

static void lv_compass_display(lv_updatable_screen_t *scr);
static void compass_update_cb(lv_updatable_screen_t *scr);

#ifdef __cplusplus
}
#endif

#endif