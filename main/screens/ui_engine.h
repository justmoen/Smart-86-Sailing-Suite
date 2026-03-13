#ifndef UI_ENGINE_H
#define UI_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>
#include "../ship_data_model.h"
#include <ui_screens.h>
#include "../ui/ui_init.h"

extern lv_updatable_screen_t engineScreen;

void init_engineScreen(void);

#ifdef __cplusplus
}
#endif

#endif