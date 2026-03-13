#ifndef UI_COMPASS_H
#define UI_COMPASS_H

#include <lvgl.h>
#include <ui_screens.h>
#include "../ui/ui_init.h"

#ifdef __cplusplus
extern "C" {
#endif


extern lv_updatable_screen_t compassScreen;

void init_compassScreen(void);

#ifdef __cplusplus
}
#endif

#endif