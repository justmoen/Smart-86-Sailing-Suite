#ifndef UI_SPEED_H
#define UI_SPEED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ui_screens.h>

extern lv_updatable_screen_t speedScreen;
void speed_process_deferred_chart_updates(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif