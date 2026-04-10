#include <ui_screens.h>
#include <ship_data_model.h>
#include <ship_data_util.h>
#include <StreamString.h>
#include "ui_tanks.h"
#include "ui_init.h"

#ifdef __cplusplus
extern "C" {
#endif

static lv_obj_t *bar_tank[MAX_TANKS];
static lv_obj_t *bar_tank_l[MAX_TANKS];

/* -------------------------------------------------- */
/* UI Creation                                        */
/* -------------------------------------------------- */

static void lv_tanks_display(lv_updatable_screen_t *scr)
{
    lv_obj_t *parent = scr->screen;

    for (int i = 0; i < MAX_TANKS; i++) {
        bar_tank[i] = lv_bar_create(parent);
        lv_obj_set_size(bar_tank[i], 50, 70);

        lv_obj_set_style_bg_color(
            bar_tank[i],
            lv_palette_lighten(LV_PALETTE_GREY, 3),
            LV_PART_MAIN
        );

        lv_obj_set_style_radius(bar_tank[i], 6, LV_PART_MAIN);
        lv_bar_set_range(bar_tank[i], 0, 100);

        bar_tank_l[i] = lv_label_create(parent);
        lv_label_set_text_static(bar_tank_l[i], "--");
    }

    /* layout */
    lv_obj_align(bar_tank[0], LV_ALIGN_CENTER, -120, -40);
    lv_obj_align(bar_tank[1], LV_ALIGN_CENTER, -40, -40);
    lv_obj_align(bar_tank[2], LV_ALIGN_CENTER, 40, -40);
    lv_obj_align(bar_tank[3], LV_ALIGN_CENTER, 120, -40);
    lv_obj_align(bar_tank[4], LV_ALIGN_CENTER, -120, 40);
    lv_obj_align(bar_tank[5], LV_ALIGN_CENTER, -40, 40);
    lv_obj_align(bar_tank[6], LV_ALIGN_CENTER, 40, 40);
    lv_obj_align(bar_tank[7], LV_ALIGN_CENTER, 120, 40);

    lv_obj_align(bar_tank_l[0], LV_ALIGN_CENTER, -120, -100);
    lv_obj_align(bar_tank_l[1], LV_ALIGN_CENTER, -40, -100);
    lv_obj_align(bar_tank_l[2], LV_ALIGN_CENTER, 40, -100);
    lv_obj_align(bar_tank_l[3], LV_ALIGN_CENTER, 120, -100);
    lv_obj_align(bar_tank_l[4], LV_ALIGN_CENTER, -120, 100);
    lv_obj_align(bar_tank_l[5], LV_ALIGN_CENTER, -40, 100);
    lv_obj_align(bar_tank_l[6], LV_ALIGN_CENTER, 40, 100);
    lv_obj_align(bar_tank_l[7], LV_ALIGN_CENTER, 120, 100);
}

/* -------------------------------------------------- */
/* Screen Update                                      */
/* -------------------------------------------------- */

static void tanks_update_cb(lv_updatable_screen_t *scr)
{
    for (int i = 0; i < MAX_TANKS; i++) {

        if (!fresh(shipDataModel.tanks.tank[i].percent_of_full.age, 7200000))
            continue;

        float pct = shipDataModel.tanks.tank[i].percent_of_full.pct;

        lv_bar_set_value(bar_tank[i], pct, LV_ANIM_OFF);

        const char *label = "--";
        lv_color_t color = lv_palette_main(LV_PALETTE_BLUE);

        switch (shipDataModel.tanks.tank[i].fluid_type) {

            case fluid_type_e::FUEL:
                label = "Fuel";
                color = lv_palette_lighten(LV_PALETTE_GREEN, 1);
                break;

            case fluid_type_e::BLACK_WATER:
                label = "Black";
                color = lv_palette_lighten(LV_PALETTE_RED, 1);
                break;

            case fluid_type_e::WASTE_WATER:
                label = "Grey";
                color = lv_palette_lighten(LV_PALETTE_RED, 1);
                break;

            case fluid_type_e::FRESH_WATER:
                label = "Fresh";
                color = lv_palette_main(LV_PALETTE_BLUE);
                break;

            case fluid_type_e::LUBRICATION:
                label = "Lube";
                color = lv_palette_lighten(LV_PALETTE_ORANGE, 1);
                break;

            case fluid_type_e::LIVE_WELL:
                label = "Live";
                color = lv_palette_main(LV_PALETTE_CYAN);
                break;

            case fluid_type_e::GAS:
                label = "Gas";
                color = lv_palette_lighten(LV_PALETTE_ORANGE, 1);
                break;

            default:
                label = "--";
                break;
        }

        lv_obj_set_style_bg_color(bar_tank[i], color, LV_PART_INDICATOR);

        lv_label_set_text(
            bar_tank_l[i],
            (String(label) + "\n" + String(pct, 1) + "%").c_str()
        );
    }
}

/* -------------------------------------------------- */
/* Screen Init                                        */
/* -------------------------------------------------- */

lv_updatable_screen_t tanksScreen = {
    .screen = nullptr,
    .created = false,
    .create_cb = lv_tanks_display,
    .update_cb = tanks_update_cb
};

#ifdef __cplusplus
} /*extern "C"*/
#endif