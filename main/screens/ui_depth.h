#ifndef UI_DEPTH_H
#define UI_DEPTH_H

// #define CONVERT_TO_FEET

#ifdef __cplusplus
extern "C" {
#endif

#include <ui_screens.h>

extern lv_updatable_screen_t depthScreen;
void depth_process_deferred_chart_updates(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif