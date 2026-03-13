#include "ui_wind.h"
#include <cstdio>
#include <ship_data_util.h>
#include <navigation.h>
#include "ui_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_updatable_screen_t windScreen;

/* LVGL objects */
static lv_obj_t *wind_display;
static lv_meter_indicator_t *indic_wind;
static lv_meter_indicator_t *indic_gwa_wind;

static lv_obj_t *wind_label;
static lv_obj_t *spd_w_label;
static lv_obj_t *gws_label;
static lv_obj_t *gwdt_label;


/* -------------------------------------------------- */
/* Helper                                             */
/* -------------------------------------------------- */

static void set_wind_value(void *indic, int32_t v)
{
    lv_meter_set_indicator_value(wind_display, (lv_meter_indicator_t *)indic, v);
}


/* -------------------------------------------------- */
/* UI Creation                                        */
/* -------------------------------------------------- */

void lv_wind_display(lv_obj_t *parent)
{
    wind_display = lv_meter_create(parent);
    lv_obj_align(wind_display, LV_ALIGN_CENTER, 0, 6);
    lv_obj_set_size(wind_display, 680, 680);

    /* scale */
    lv_meter_scale_t *scale = lv_meter_add_scale(wind_display);
    lv_meter_set_scale_ticks(wind_display, scale, 37, 4, 15, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_range(wind_display, scale, -180, 180, 360, 90);

    lv_meter_scale_t *scale2 = lv_meter_add_scale(wind_display);
    lv_meter_set_scale_ticks(wind_display, scale2, 12, 0, 0, lv_palette_main(LV_PALETTE_GREY));

    lv_obj_set_style_text_font(wind_display, &lv_font_montserrat_26, LV_PART_TICKS);

    lv_meter_set_scale_major_ticks(
        wind_display,
        scale2,
        1,
        3,
        14,
        lv_palette_main(LV_PALETTE_GREY),
        14);

    lv_meter_set_scale_range(wind_display, scale2, -150, 180, 330, 120);

    /* red arc */
    indic_wind = lv_meter_add_arc(wind_display, scale, 4, lv_palette_main(LV_PALETTE_RED), 2);
    lv_meter_set_indicator_start_value(wind_display, indic_wind, -60);
    lv_meter_set_indicator_end_value(wind_display, indic_wind, -20);

    indic_wind = lv_meter_add_scale_lines(
        wind_display,
        scale,
        lv_palette_main(LV_PALETTE_RED),
        lv_palette_main(LV_PALETTE_RED),
        false,
        0);

    lv_meter_set_indicator_start_value(wind_display, indic_wind, -60);
    lv_meter_set_indicator_end_value(wind_display, indic_wind, -20);

    /* green arc */
    indic_wind = lv_meter_add_arc(wind_display, scale, 4, lv_palette_main(LV_PALETTE_GREEN), 2);
    lv_meter_set_indicator_start_value(wind_display, indic_wind, 20);
    lv_meter_set_indicator_end_value(wind_display, indic_wind, 60);

    indic_wind = lv_meter_add_scale_lines(
        wind_display,
        scale,
        lv_palette_main(LV_PALETTE_GREEN),
        lv_palette_main(LV_PALETTE_GREEN),
        false,
        0);

    lv_meter_set_indicator_start_value(wind_display, indic_wind, 20);
    lv_meter_set_indicator_end_value(wind_display, indic_wind, 60);

    /* apparent wind needle */
    indic_wind = lv_meter_add_needle_line(
        wind_display,
        scale,
        10,
        lv_palette_main(LV_PALETTE_GREY),
        -10);

    /* ground wind needle */
    indic_gwa_wind = lv_meter_add_needle_line(
        wind_display,
        scale,
        10,
        lv_palette_main(LV_PALETTE_ORANGE),
        -48);


    /* Labels */

    wind_label = lv_label_create(parent);
    lv_obj_align(wind_label, LV_ALIGN_TOP_LEFT, 5, 2);

#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(wind_label, &lv_font_montserrat_30, 0);
#endif

    lv_label_set_text_static(wind_label, "AWS: --\nkt");


    spd_w_label = lv_label_create(parent);
    lv_obj_align(spd_w_label, LV_ALIGN_TOP_RIGHT, -5, 2);

#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(spd_w_label, &lv_font_montserrat_30, 0);
#endif

    lv_label_set_text_static(spd_w_label, "SPD: --\nkt");


    gws_label = lv_label_create(parent);
    lv_obj_align(gws_label, LV_ALIGN_BOTTOM_LEFT, 5, -2);

#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(gws_label, &lv_font_montserrat_30, 0);
#endif

    lv_label_set_text_static(gws_label, "GWS:\n-- kt");


    gwdt_label = lv_label_create(parent);
    lv_obj_align(gwdt_label, LV_ALIGN_BOTTOM_RIGHT, -5, -2);

#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(gwdt_label, &lv_font_montserrat_30, 0);
#endif

    lv_label_set_text_static(gwdt_label, "GWD:\n--" LV_SYMBOL_DEGREES "t");
}


/* -------------------------------------------------- */
/* Screen Update                                      */
/* -------------------------------------------------- */

void wind_update_cb(void)
{
    char buf[64];

    if (fresh(shipDataModel.environment.wind.apparent_wind_speed.age))
        snprintf(buf, sizeof(buf), "AWS: %.1f\nkt",
                 shipDataModel.environment.wind.apparent_wind_speed.kn);
    else
        snprintf(buf, sizeof(buf), "AWS: --\nkt");

    lv_label_set_text(wind_label, buf);


    if (fresh(shipDataModel.navigation.speed_through_water.age))
        snprintf(buf, sizeof(buf), "SPD: %.1f\nkt",
                 shipDataModel.navigation.speed_through_water.kn);
    else
        snprintf(buf, sizeof(buf), "SPD: --\nkt");

    lv_label_set_text(spd_w_label, buf);


    if (fresh(shipDataModel.environment.wind.ground_wind_speed.age))
        snprintf(buf, sizeof(buf), "GWS:\n%.1f kt",
                 shipDataModel.environment.wind.ground_wind_speed.kn);
    else
        snprintf(buf, sizeof(buf), "GWS:\n-- kt");

    lv_label_set_text(gws_label, buf);


    if (fresh(shipDataModel.environment.wind.ground_wind_dir_true.age))
        snprintf(buf, sizeof(buf), "GWD:\n%.0f" LV_SYMBOL_DEGREES "t",
                 shipDataModel.environment.wind.ground_wind_dir_true.deg);
    else
        snprintf(buf, sizeof(buf), "GWD:\n--" LV_SYMBOL_DEGREES "t");

    lv_label_set_text(gwdt_label, buf);


    set_wind_value(
        indic_wind,
        fresh(shipDataModel.environment.wind.apparent_wind_angle.age, 1)
            ? shipDataModel.environment.wind.apparent_wind_angle.deg
            : 0);

    set_wind_value(
        indic_gwa_wind,
        fresh(shipDataModel.environment.wind.ground_wind_angle.age, 1)
            ? shipDataModel.environment.wind.ground_wind_angle.deg
            : 0);
}


/* -------------------------------------------------- */
/* Screen Init                                        */
/* -------------------------------------------------- */

void init_windScreen(void)
{
    windScreen.screen = lv_obj_create(NULL);
    windScreen.init_cb = lv_wind_display;
    windScreen.update_cb = wind_update_cb;
}

#ifdef __cplusplus
}
#endif