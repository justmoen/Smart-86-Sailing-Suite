#ifndef UI_GPS_H
#define UI_GPS_H

#include "ui_screens.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_updatable_screen_t gpsScreen;

void init_gpsScreen(void);
void lv_gps_display(lv_obj_t *parent);
void gps_update_cb(void);

#ifdef __cplusplus
}
#endif

#endif
