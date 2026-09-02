#ifndef UI_GPS_H
#define UI_GPS_H

#include <ui_init.h>
#include <StreamString.h>
#include <ctime>
#include <navigation.h>
#include "ui_gps.h"
#include "ui_screens.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <ship_data_model.h>
#include <ship_data_util.h>

  static lv_obj_t *gps_time_label;
  static lv_obj_t *gps_cogt_label;
  static lv_obj_t *gps_cogm_label;
  static lv_obj_t *gps_lat_label;
  static lv_obj_t *gps_lon_label;

  /**
   * A GPS display 
   */
  static void lv_gps_display(lv_updatable_screen_t *scr) {

    lv_obj_t *main_label = lv_label_create(scr->screen);
    lv_obj_align(main_label, LV_ALIGN_CENTER, 0, -105);
    lv_obj_set_style_text_font(main_label, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_label_set_recolor(main_label, true);
    lv_label_set_text_static(main_label, "GPS  #0080ff " LV_SYMBOL_GPS " #");

    gps_time_label = lv_label_create(scr->screen);
    lv_obj_align(gps_time_label, LV_ALIGN_TOP_LEFT, 10, 40);
#if LV_FONT_MONTSERRAT_28
    lv_obj_set_style_text_font(gps_time_label, &lv_font_montserrat_28, LV_PART_MAIN);
#endif
    lv_label_set_text_static(gps_time_label, "GPS Time:         --");

    gps_lat_label = lv_label_create(scr->screen);
    lv_obj_align(gps_lat_label, LV_ALIGN_TOP_LEFT, 10, 80);
#if LV_FONT_MONTSERRAT_28
    lv_obj_set_style_text_font(gps_lat_label, &lv_font_montserrat_28, LV_PART_MAIN);
#endif
    lv_label_set_text_static(gps_lat_label, "LAT:                    --");

    gps_lon_label = lv_label_create(scr->screen);
    lv_obj_align(gps_lon_label, LV_ALIGN_TOP_LEFT, 10, 120);
#if LV_FONT_MONTSERRAT_28
    lv_obj_set_style_text_font(gps_lon_label, &lv_font_montserrat_28, LV_PART_MAIN);
#endif
    lv_label_set_text_static(gps_lon_label, "LON:                   --");

    gps_cogt_label = lv_label_create(scr->screen);
    lv_obj_align(gps_cogt_label, LV_ALIGN_TOP_LEFT, 10, 160);
#if LV_FONT_MONTSERRAT_28
    lv_obj_set_style_text_font(gps_cogt_label, &lv_font_montserrat_28, LV_PART_MAIN);
#endif
    lv_label_set_text_static(gps_cogt_label, "COGT:                 --");

    gps_cogm_label = lv_label_create(scr->screen);
    lv_obj_align(gps_cogm_label, LV_ALIGN_TOP_LEFT, 10, 200);
#if LV_FONT_MONTSERRAT_28
    lv_obj_set_style_text_font(gps_cogm_label, &lv_font_montserrat_28, LV_PART_MAIN);
#endif
    lv_label_set_text_static(gps_cogm_label, "COGM:                 --");
  }

  static String lat_print(float lat_deg) {
    return String(lat_deg, 6) += LV_SYMBOL_DEGREES;
  }

  static String lon_print(float lon_deg) {
    return String(lon_deg, 6) += LV_SYMBOL_DEGREES;
  }

  static String time_format(tm &t) {
    char bufferTime[9];
    sprintf(bufferTime, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    return String(bufferTime);
  }

  static void gps_update_cb(lv_updatable_screen_t *scr) {
    lv_label_set_text(gps_time_label,
                      (String("GPS Time:          ")
                       += (fresh(shipDataModel.environment.time_gps.age) ? time_format(shipDataModel.environment.time_gps.t) : String("--")))
                        .c_str());
    lv_label_set_text(gps_lat_label,
                      (String("LAT:                      ")
                       += (fresh(shipDataModel.navigation.position.lat.age) ? lat_print(shipDataModel.navigation.position.lat.deg) : String("--")))
                        .c_str());
    lv_label_set_text(gps_lon_label,
                      (String("LON:                    ")
                       += (fresh(shipDataModel.navigation.position.lon.age) ? lon_print(shipDataModel.navigation.position.lon.deg) : String("--")))
                        .c_str());
    lv_label_set_text(gps_cogt_label,
                      (String("COGT:                  ")
                       += (fresh(shipDataModel.navigation.course_over_ground_true.age) ? String(shipDataModel.navigation.course_over_ground_true.deg, 1) += LV_SYMBOL_DEGREES : String("--")))
                        .c_str());
    lv_label_set_text(gps_cogm_label,
                      (String("COGM:                 ")
                       += (fresh(shipDataModel.navigation.course_over_ground_mag.age) ? String(shipDataModel.navigation.course_over_ground_mag.deg, 1) += LV_SYMBOL_DEGREES : String("--")))
                        .c_str());
  }

lv_updatable_screen_t gpsScreen = {
    .screen = nullptr,
    .created = false,
    .create_cb = lv_gps_display,
    .update_cb = gps_update_cb
};

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif