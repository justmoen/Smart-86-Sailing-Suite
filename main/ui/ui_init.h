#ifndef UI_INIT_H
#define UI_INIT_H

#include <lvgl.h>

#define LV_SYMBOL_DOUBLE_LEFT LV_SYMBOL_LEFT " " LV_SYMBOL_LEFT
#define LV_SYMBOL_DOUBLE_RIGHT LV_SYMBOL_RIGHT " " LV_SYMBOL_RIGHT
#define LV_SYMBOL_DEGREES "\xC2\xB0"

#ifdef __cplusplus
extern "C" {
#endif

static unsigned long last_touched;

void apply_screen_style(lv_obj_t *scr);
void apply_meter_style(lv_obj_t *meter);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
