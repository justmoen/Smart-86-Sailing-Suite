#ifndef UI_DEPTH_H
#define UI_DEPTH_H

// #define CONVERT_TO_FEET

#ifdef __cplusplus
extern "C" {
#endif

#include <ui_screens.h>
#include "ui/chart_data_history.h">

extern lv_updatable_screen_t depthScreen;

struct BestDepth {
    float m;
    bool valid;
};

BestDepth get_best_depth(void);
extern ChartDataHistory *depth_history;
void depth_queue_chart_data(void);
void depth_process_deferred_chart_updates(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif