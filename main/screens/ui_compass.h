#ifndef UI_COMPASS_H
#define UI_COMPASS_H

#include "ui_screens.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_updatable_screen_t compassScreen;

void init_compassScreen(void);
void lv_compass_display(lv_obj_t *parent);
void compass_update_cb(void);

#ifdef __cplusplus
}
#endif

#endif