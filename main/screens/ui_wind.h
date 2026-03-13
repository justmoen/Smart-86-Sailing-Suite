#ifndef UI_WIND_H
#define UI_WIND_H

#include <lvgl.h>
#include "../ship_data_model.h"
#include <ui_screens.h>
#include "../ui/ui_init.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_updatable_screen_t windScreen;

void init_windScreen(void);

#ifdef __cplusplus
}
#endif

#endif