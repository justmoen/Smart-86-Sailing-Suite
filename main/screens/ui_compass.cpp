#include "ui_compass.h"
#include <navigation.h>
#include <cstdio>
#include <ship_data_util.h>
#include <ui_init.h>

#ifdef __cplusplus
extern "C" {
#endif

lv_updatable_screen_t compassScreen;

/* LVGL objects */

static lv_obj_t *compass_display;
static lv_obj_t *compass_l;
static lv_obj_t *compass_hdt_l;
static lv_obj_t *compass_cogt_l;
static lv_obj_t *compass_mag_var_l;

static lv_obj_t *labelScont;
static lv_obj_t *labelNcont;
static lv_obj_t *labelEcont;
static lv_obj_t *labelWcont;

static lv_meter_scale_t *scale_compass;
static lv_meter_scale_t *scale_compass_maj;

static int16_t last_heading = -1;
static uint32_t last_compass_upd = 0;


/* -------------------------------------------------- */
/* UI Creation                                        */
/* -------------------------------------------------- */

void lv_compass_display(lv_obj_t *parent)
{
    compass_display = lv_meter_create(parent);

    lv_obj_remove_style(compass_display, NULL, LV_PART_MAIN);
    lv_obj_remove_style(compass_display, NULL, LV_PART_INDICATOR);

    lv_obj_set_size(compass_display, 680, 680);
    lv_obj_center(compass_display);

    scale_compass = lv_meter_add_scale(compass_display);
    lv_meter_set_scale_ticks(compass_display, scale_compass, 73, 1, 12, lv_palette_main(LV_PALETTE_GREY));

    scale_compass_maj = lv_meter_add_scale(compass_display);

#if LV_FONT_MONTSERRAT_26
    lv_obj_set_style_text_font(compass_display, &lv_font_montserrat_26, LV_PART_TICKS);
#endif

    lv_meter_set_scale_ticks(compass_display, scale_compass_maj, 12, 2, 15, lv_palette_main(LV_PALETTE_GREY));


    /* N label */

    labelNcont = lv_obj_create(parent);
    lv_obj_set_size(labelNcont, 42, 42);
    lv_obj_set_style_pad_all(labelNcont, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(labelNcont, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_align(labelNcont, LV_ALIGN_CENTER, 0, -48);

    lv_obj_t *labelN = lv_label_create(labelNcont);

#if LV_FONT_MONTSERRAT_32
    lv_obj_set_style_text_font(labelN, &lv_font_montserrat_32, 0);
#endif

    lv_label_set_text_static(labelN, "N");
    lv_obj_center(labelN);

    lv_obj_set_style_transform_pivot_x(labelNcont, 21, 0);
    lv_obj_set_style_transform_pivot_y(labelNcont, 21 + 48, 0);


    /* S label */

    labelScont = lv_obj_create(parent);
    lv_obj_set_size(labelScont, 42, 42);
    lv_obj_set_style_pad_all(labelScont, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(labelScont, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
    lv_obj_align(labelScont, LV_ALIGN_CENTER, 0, -48);

    lv_obj_t *labelS = lv_label_create(labelScont);

#if LV_FONT_MONTSERRAT_32
    lv_obj_set_style_text_font(labelS, &lv_font_montserrat_32, 0);
#endif

    lv_label_set_text_static(labelS, "S");
    lv_obj_center(labelS);

    lv_obj_set_style_transform_pivot_x(labelScont, 21, 0);
    lv_obj_set_style_transform_pivot_y(labelScont, 21 + 48, 0);


    /* E label */

    labelEcont = lv_obj_create(parent);
    lv_obj_set_size(labelEcont, 42, 42);
    lv_obj_set_style_pad_all(labelEcont, 2, LV_PART_MAIN);
    lv_obj_align(labelEcont, LV_ALIGN_CENTER, 0, -48);

    lv_obj_t *labelE = lv_label_create(labelEcont);

#if LV_FONT_MONTSERRAT_32
    lv_obj_set_style_text_font(labelE, &lv_font_montserrat_32, 0);
#endif

    lv_label_set_text_static(labelE, "E");
    lv_obj_center(labelE);

    lv_obj_set_style_transform_pivot_x(labelEcont, 21, 0);
    lv_obj_set_style_transform_pivot_y(labelEcont, 21 + 48, 0);


    /* W label */

    labelWcont = lv_obj_create(parent);
    lv_obj_set_size(labelWcont, 42, 42);
    lv_obj_set_style_pad_all(labelWcont, 2, LV_PART_MAIN);
    lv_obj_align(labelWcont, LV_ALIGN_CENTER, 0, -48);

    lv_obj_t *labelW = lv_label_create(labelWcont);

#if LV_FONT_MONTSERRAT_32
    lv_obj_set_style_text_font(labelW, &lv_font_montserrat_32, 0);
#endif

    lv_label_set_text_static(labelW, "W");
    lv_obj_center(labelW);

    lv_obj_set_style_transform_pivot_x(labelWcont, 21, 0);
    lv_obj_set_style_transform_pivot_y(labelWcont, 21 + 48, 0);


    /* Static rotations */

    lv_obj_set_style_transform_angle(labelScont, 180 * 10, 0);
    lv_obj_set_style_transform_angle(labelEcont, 90 * 10, 0);
    lv_obj_set_style_transform_angle(labelWcont, 270 * 10, 0);


    /* heading marker */

    lv_obj_t *compass_mark_l = lv_label_create(parent);
    lv_label_set_text_static(compass_mark_l, LV_SYMBOL_DOWN);
    lv_obj_align(compass_mark_l, LV_ALIGN_CENTER, 0, -100);


    compass_l = lv_label_create(parent);

#if LV_FONT_MONTSERRAT_26
    lv_obj_set_style_text_font(compass_l, &lv_font_montserrat_26, 0);
#endif

    lv_label_set_text_static(compass_l, "--" LV_SYMBOL_DEGREES);
    lv_obj_center(compass_l);


    lv_meter_set_scale_range(compass_display, scale_compass, 0, 72, 360, 270);
    lv_meter_set_scale_range(compass_display, scale_compass_maj, 1, 12, 330, 300);


    /* info labels */

    compass_hdt_l = lv_label_create(parent);
    lv_label_set_text_static(compass_hdt_l, "HDT: --" LV_SYMBOL_DEGREES);
    lv_obj_align(compass_hdt_l, LV_ALIGN_TOP_LEFT, 2, 2);

#if LV_FONT_MONTSERRAT_26
    lv_obj_set_style_text_font(compass_hdt_l, &lv_font_montserrat_26, 0);
#endif


    compass_cogt_l = lv_label_create(parent);
    lv_label_set_text_static(compass_cogt_l, "COGT: --" LV_SYMBOL_DEGREES);
    lv_obj_align(compass_cogt_l, LV_ALIGN_TOP_RIGHT, -2, 2);

#if LV_FONT_MONTSERRAT_26
    lv_obj_set_style_text_font(compass_cogt_l, &lv_font_montserrat_26, 0);
#endif


    compass_mag_var_l = lv_label_create(parent);
    lv_label_set_text_static(compass_mag_var_l, "Var:\n--" LV_SYMBOL_DEGREES);
    lv_obj_align(compass_mag_var_l, LV_ALIGN_BOTTOM_LEFT, 2, -2);

#if LV_FONT_MONTSERRAT_26
    lv_obj_set_style_text_font(compass_mag_var_l, &lv_font_montserrat_26, 0);
#endif
}


/* -------------------------------------------------- */
/* Screen Update                                      */
/* -------------------------------------------------- */

void compass_update_cb(void)
{
    uint32_t now = lv_tick_get();

    if(now - last_compass_upd < 500)
        return;

    int16_t heading =
        fresh(shipDataModel.navigation.heading_mag.age, 1)
        ? shipDataModel.navigation.heading_mag.deg
        : 0;

    if(heading != last_heading)
    {
        int rot = 360 - heading;

        lv_meter_set_scale_range(compass_display, scale_compass, 0, 72, 360, 270 + rot);
        lv_meter_set_scale_range(compass_display, scale_compass_maj, 1, 12, 330, 300 + rot);

        lv_obj_set_style_transform_angle(labelNcont, rot * 10, 0);
        lv_obj_set_style_transform_angle(labelScont, (180 + rot) * 10, 0);
        lv_obj_set_style_transform_angle(labelEcont, (90 + rot) * 10, 0);
        lv_obj_set_style_transform_angle(labelWcont, (270 + rot) * 10, 0);

        char buf[32];

        snprintf(buf, sizeof(buf), "%d" LV_SYMBOL_DEGREES, heading);
        lv_label_set_text(compass_l, buf);

        if (fresh(shipDataModel.navigation.heading_true.age))
        {
            snprintf(buf, sizeof(buf), "HDT: %d" LV_SYMBOL_DEGREES,
                     (int)shipDataModel.navigation.heading_true.deg);
            lv_label_set_text(compass_hdt_l, buf);
        }

        if (fresh(shipDataModel.navigation.course_over_ground_true.age))
        {
            snprintf(buf, sizeof(buf), "COGT: %d" LV_SYMBOL_DEGREES,
                     (int)shipDataModel.navigation.course_over_ground_true.deg);
            lv_label_set_text(compass_cogt_l, buf);
        }

        if (fresh(shipDataModel.navigation.mag_var.age))
        {
            snprintf(buf, sizeof(buf), "Var:\n%.1f" LV_SYMBOL_DEGREES,
                     shipDataModel.navigation.mag_var.deg);
            lv_label_set_text(compass_mag_var_l, buf);
        }

        last_heading = heading;
    }

    last_compass_upd = now;
}


/* -------------------------------------------------- */
/* Screen Init                                        */
/* -------------------------------------------------- */

void init_compassScreen(void)
{
    compassScreen.screen = lv_obj_create(NULL);
    compassScreen.init_cb = lv_compass_display;
    compassScreen.update_cb = compass_update_cb;
}

#ifdef __cplusplus
}
#endif