#include <ui_screens.h>
#include <ship_data_model.h>
#include <ship_data_util.h>
#include <StreamString.h>
#include "ui_tanks.h"
#include "ui_init.h"
#include <signalk_path_config.h>

#ifdef __cplusplus
extern "C" {
#endif

static lv_obj_t *bar_tank[MAX_TANKS];
static lv_obj_t *bar_tank_l[MAX_TANKS];

static void tanks_destroy_cb(lv_updatable_screen_t *scr) {
  for (int i = 0; i < MAX_TANKS; i++) {
    if (bar_tank[i]) {
      lv_obj_delete(bar_tank[i]);
      bar_tank[i] = nullptr;
    }
    if (bar_tank_l[i]) {
      lv_obj_delete(bar_tank_l[i]);
      bar_tank_l[i] = nullptr;
    }
  }
}

/* -------------------------------------------------- */
/* UI Creation                                        */
/* -------------------------------------------------- */

static void lv_tanks_display(lv_updatable_screen_t *scr)
{
    const auto& config = get_signalk_path_config();
    lv_obj_t *parent = scr->screen;
    
    int num_tanks = config.num_tanks;

    // Calculate tank dimensions based on number of tanks
    // For 2 tanks: large (280x320), for 4 tanks: medium (200x220), for 6+ tanks: smaller
    int tank_width, tank_height;
    if (num_tanks == 1) {
        tank_width = 320;
        tank_height = 400;
    } else if (num_tanks == 2) {
        tank_width = 280;
        tank_height = 340;
    } else if (num_tanks == 3) {
        tank_width = 240;
        tank_height = 280;
    } else if (num_tanks <= 4) {
        tank_width = 200;
        tank_height = 240;
    } else if (num_tanks <= 6) {
        tank_width = 160;
        tank_height = 220;
    } else {
        tank_width = 140;
        tank_height = 200;
    }

    for (int i = 0; i < MAX_TANKS; i++) {
        bar_tank[i] = lv_bar_create(parent);
        lv_obj_set_size(bar_tank[i], tank_width, tank_height);

        lv_obj_set_style_bg_color(
            bar_tank[i],
            lv_palette_lighten(LV_PALETTE_GREY, 3),
            LV_PART_MAIN
        );

        lv_obj_set_style_radius(bar_tank[i], 6, LV_PART_MAIN);
        lv_obj_set_style_radius(bar_tank[i], 0, LV_PART_INDICATOR);
        lv_bar_set_range(bar_tank[i], 0, 100);

        bar_tank_l[i] = lv_label_create(parent);
        lv_label_set_text_static(bar_tank_l[i], "--");
        lv_obj_set_width(bar_tank_l[i], 120);
        lv_label_set_long_mode(bar_tank_l[i], LV_LABEL_LONG_WRAP);

#if LV_FONT_MONTSERRAT_28
        lv_obj_set_style_text_font(bar_tank_l[i], &lv_font_montserrat_28, 0);
#endif
        
        // Hide tanks that are not configured
        if (i >= config.num_tanks) {
            lv_obj_add_flag(bar_tank[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(bar_tank_l[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Dynamic layout based on number of tanks */
    
    // Calculate positions based on number of tanks
    // Available space: 680x480 (720x720 display with title/margin)
    // Center at 360x240, usable area approximately ±300 horizontal, ±180 vertical
    
    int x_positions[MAX_TANKS];
    int y_positions[MAX_TANKS];
    int x_label_offsets[MAX_TANKS];
    int y_label_offsets[MAX_TANKS];
    
    if (num_tanks == 1) {
        // Single tank centered
        x_positions[0] = 0; y_positions[0] = 0;
        x_label_offsets[0] = 0; y_label_offsets[0] = -210;
    } else if (num_tanks == 2) {
        // Two tanks side by side
        x_positions[0] = -180; y_positions[0] = 0;
        x_positions[1] = 180; y_positions[1] = 0;
        x_label_offsets[0] = -180; y_label_offsets[0] = -200;
        x_label_offsets[1] = 180; y_label_offsets[1] = -200;
    } else if (num_tanks == 3) {
        // Three tanks: 2 on bottom, 1 on top centered
        x_positions[0] = 0; y_positions[0] = -100;
        x_positions[1] = -140; y_positions[1] = 120;
        x_positions[2] = 140; y_positions[2] = 120;
        x_label_offsets[0] = 0; y_label_offsets[0] = -220;
        x_label_offsets[1] = -140; y_label_offsets[1] = 220;
        x_label_offsets[2] = 140; y_label_offsets[2] = 220;
    } else if (num_tanks <= 4) {
        // Four tanks in 2x2 grid
        x_positions[0] = -130; y_positions[0] = -100;
        x_positions[1] = 130; y_positions[1] = -100;
        x_positions[2] = -130; y_positions[2] = 100;
        x_positions[3] = 130; y_positions[3] = 100;
        x_label_offsets[0] = -130; y_label_offsets[0] = -180;
        x_label_offsets[1] = 130; y_label_offsets[1] = -180;
        x_label_offsets[2] = -130; y_label_offsets[2] = 180;
        x_label_offsets[3] = 130; y_label_offsets[3] = 180;
    } else if (num_tanks <= 6) {
        // Five or six tanks in 3 columns, 2 rows
        int x_spacing = 160;  // Adjust for 3 columns
        int y_spacing = 140;
        
        // Top row: 3 tanks
        for (int i = 0; i < 3 && i < num_tanks; i++) {
            x_positions[i] = (i - 1) * x_spacing;
            y_positions[i] = -y_spacing / 2;
            x_label_offsets[i] = x_positions[i];
            y_label_offsets[i] = y_positions[i] - 160;
        }
        // Bottom row: up to 3 tanks
        for (int i = 3; i < num_tanks && i < 6; i++) {
            x_positions[i] = (i - 4) * x_spacing;
            y_positions[i] = y_spacing / 2;
            x_label_offsets[i] = x_positions[i];
            y_label_offsets[i] = y_positions[i] + 160;
        }
    } else {
        // Seven or eight tanks in 4 columns, 2 rows (max configuration)
        int x_spacing = 130;  // Tighter spacing for 4 columns
        int y_spacing = 140;
        
        // Top row: 4 tanks
        for (int i = 0; i < 4 && i < num_tanks; i++) {
            x_positions[i] = (i - 1.5) * x_spacing;
            y_positions[i] = -y_spacing / 2;
            x_label_offsets[i] = x_positions[i];
            y_label_offsets[i] = y_positions[i] - 140;
        }
        // Bottom row: up to 4 tanks
        for (int i = 4; i < num_tanks && i < 8; i++) {
            x_positions[i] = (i - 5.5) * x_spacing;
            y_positions[i] = y_spacing / 2;
            x_label_offsets[i] = x_positions[i];
            y_label_offsets[i] = y_positions[i] + 140;
        }
    }
    
    // Apply calculated positions to all tanks
    for (int i = 0; i < MAX_TANKS; i++) {
        lv_obj_align(bar_tank[i], LV_ALIGN_CENTER, x_positions[i], y_positions[i]);
        lv_obj_align(bar_tank_l[i], LV_ALIGN_CENTER, x_label_offsets[i], y_label_offsets[i]);
    }
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
    .update_cb = tanks_update_cb,
    .destroy_cb = tanks_destroy_cb
};

#ifdef __cplusplus
} /*extern "C"*/
#endif