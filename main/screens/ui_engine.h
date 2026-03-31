#ifndef UI_ENGINE_H
#define UI_ENGINE_H

#include "lvgl.h"
#include "ui_screens.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_updatable_screen_t engineScreen;

#ifdef __cplusplus
} /* extern "C" */
#endif

// C++ declarations
#ifdef __cplusplus
#include <Arduino.h>

// LVGL objects (defined in cpp)
extern lv_obj_t *engine_rpm_meter;
extern lv_meter_indicator_t *engine_rpm_indic;

extern lv_obj_t *oil_press_meter;
extern lv_meter_indicator_t *oil_press_indic;

extern lv_obj_t *eng_temp_meter;
extern lv_meter_indicator_t *eng_temp_indic;

extern lv_obj_t *eng_sog_label;
extern lv_obj_t *eng_alternator_label;

#endif // __cplusplus

#endif // UI_ENGINE_H