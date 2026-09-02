#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

  static lv_obj_t *lcd_conf_obj;
  static lv_obj_t *lcd_slider_label;

  void lv_lcd_settings(lv_obj_t *parent);
  void save_page(int page);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
