#ifndef UI_HEEL_H
#define UI_HEEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "ui_manager.h"

extern lv_updatable_screen_t heelScreen;

void init_heelScreen(void);

#ifdef __cplusplus
}
#endif

#endif