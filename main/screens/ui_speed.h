#ifndef UI_SPEED_H
#define UI_SPEED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ui_screens.h>
#include "ui/chart_data_history.h"

extern lv_updatable_screen_t speedScreen;
extern ChartDataHistory *speed_history;
void speed_queue_chart_data(void);
void speed_process_deferred_chart_updates(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif