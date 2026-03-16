#ifndef UI_GPS_H
#define UI_GPS_H

#include "ui_screens.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_updatable_screen_t gpsScreen;

static void lv_gps_display(lv_updatable_screen_t *scr);
static void gps_update_cb(lv_updatable_screen_t *scr);

#ifdef __cplusplus
}
#endif

#endif
