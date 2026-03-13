#ifndef UI_COMPASS_H
#define UI_COMPASS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>
#include "../ship_data_model.h"
#include <ui_screens.h>
#include "../ui/ui_init.h"

extern lv_updatable_screen_t compassScreen;

void init_compassScreen(void);

#ifdef __cplusplus
}
#endif

#endif