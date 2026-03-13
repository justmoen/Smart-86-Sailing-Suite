#include "ui_engine.h"
#include "ship_data_model.h"
#include "ship_data_util.h"
#include <ui_init.h>
#include "ui_screens.h"

// LVGL objects
lv_updatable_screen_t engineScreen;

lv_obj_t *engine_rpm_meter = nullptr;
lv_meter_indicator_t *engine_rpm_indic = nullptr;

lv_obj_t *oil_press_meter = nullptr;
lv_meter_indicator_t *oil_press_indic = nullptr;

lv_obj_t *eng_temp_meter = nullptr;
lv_meter_indicator_t *eng_temp_indic = nullptr;

lv_obj_t *eng_sog_label = nullptr;
lv_obj_t *eng_alternator_label = nullptr;

// Helper to set RPM
static void set_engine_rpm_value(void *indic, int32_t v) {
    lv_meter_set_indicator_value(engine_rpm_meter, (lv_meter_indicator_t *)indic, v);
}

// Display initialization
void lv_engine_display(lv_obj_t *parent) {
    engine_rpm_meter = lv_meter_create(parent);
    lv_obj_center(engine_rpm_meter);
    lv_obj_set_size(engine_rpm_meter, 680, 680);

    lv_obj_t *main_label = lv_label_create(parent);
    lv_obj_align(main_label, LV_ALIGN_CENTER, 0, 50);
#if LV_FONT_MONTSERRAT_22
    lv_obj_set_style_text_font(main_label, &lv_font_montserrat_22, 0);
#endif
    lv_label_set_text_static(main_label, "RPM\nx100");

    // Add scale
    lv_meter_scale_t *scale = lv_meter_add_scale(engine_rpm_meter);
    lv_meter_set_scale_ticks(engine_rpm_meter, scale, 31, 2, 10, lv_palette_main(LV_PALETTE_GREY));
#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(engine_rpm_meter, &lv_font_montserrat_30, LV_PART_TICKS);
#endif
    lv_meter_set_scale_major_ticks(engine_rpm_meter, scale, 5, 4, 15, lv_palette_main(LV_PALETTE_GREY), 10);
    lv_meter_set_scale_range(engine_rpm_meter, scale, 0, 60, 240, 150);

    // Blue arc and ticks
    engine_rpm_indic = lv_meter_add_arc(engine_rpm_meter, scale, 3, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_meter_set_indicator_start_value(engine_rpm_meter, engine_rpm_indic, 0);
    lv_meter_set_indicator_end_value(engine_rpm_meter, engine_rpm_indic, 20);
    engine_rpm_indic = lv_meter_add_scale_lines(
        engine_rpm_meter, scale, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_BLUE), true, 0);
    lv_meter_set_indicator_start_value(engine_rpm_meter, engine_rpm_indic, 0);
    lv_meter_set_indicator_end_value(engine_rpm_meter, engine_rpm_indic, 20);

    // Red arc and ticks
    engine_rpm_indic = lv_meter_add_arc(engine_rpm_meter, scale, 3, lv_palette_main(LV_PALETTE_RED), 0);
    lv_meter_set_indicator_start_value(engine_rpm_meter, engine_rpm_indic, 40);
    lv_meter_set_indicator_end_value(engine_rpm_meter, engine_rpm_indic, 60);
    engine_rpm_indic = lv_meter_add_scale_lines(
        engine_rpm_meter, scale, lv_palette_main(LV_PALETTE_RED), lv_palette_main(LV_PALETTE_RED), true, 0);
    lv_meter_set_indicator_start_value(engine_rpm_meter, engine_rpm_indic, 40);
    lv_meter_set_indicator_end_value(engine_rpm_meter, engine_rpm_indic, 60);

    // Needle
    engine_rpm_indic = lv_meter_add_needle_line(engine_rpm_meter, scale, 4, lv_palette_main(LV_PALETTE_GREY), -10);

    // Oil pressure
    oil_press_meter = lv_meter_create(parent);
    lv_obj_align(oil_press_meter, LV_ALIGN_CENTER, -125, 120);
    lv_obj_set_size(oil_press_meter, 160, 160);
    lv_obj_remove_style(oil_press_meter, NULL, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(oil_press_meter, 0, LV_PART_MAIN);
    lv_meter_scale_t *oil_press_scale = lv_meter_add_scale(oil_press_meter);
    lv_meter_set_scale_ticks(oil_press_meter, oil_press_scale, 10, 2, 7, lv_palette_main(LV_PALETTE_GREY));
#if LV_FONT_MONTSERRAT_26
    lv_obj_set_style_text_font(oil_press_meter, &lv_font_montserrat_26, LV_PART_TICKS);
#endif
    lv_meter_set_scale_major_ticks(oil_press_meter, oil_press_scale, 3, 2, 7, lv_palette_main(LV_PALETTE_GREY), 10);
    lv_meter_set_scale_range(oil_press_meter, oil_press_scale, 0, 90, 270, 90);
    oil_press_indic = lv_meter_add_arc(oil_press_meter, oil_press_scale, 3, lv_palette_main(LV_PALETTE_BLUE), 1);
    lv_meter_set_indicator_start_value(oil_press_meter, oil_press_indic, 0);

    lv_obj_t *oil_press_label = lv_label_create(parent);
    lv_obj_align(oil_press_label, LV_ALIGN_BOTTOM_LEFT, 80, -2);
#if LV_FONT_MONTSERRAT_32
    lv_obj_set_style_text_font(oil_press_label, &lv_font_montserrat_32, 0);
#endif
    lv_label_set_text_static(oil_press_label, "psi");

    // Engine temp
    eng_temp_meter = lv_meter_create(parent);
    lv_obj_align(eng_temp_meter, LV_ALIGN_CENTER, 125, 120);
    lv_obj_set_size(eng_temp_meter, 160, 160);
    lv_obj_remove_style(eng_temp_meter, NULL, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(eng_temp_meter, 0, LV_PART_MAIN);
    lv_meter_scale_t *eng_temp_scale = lv_meter_add_scale(eng_temp_meter);
    lv_meter_set_scale_ticks(eng_temp_meter, eng_temp_scale, 13, 2, 7, lv_palette_main(LV_PALETTE_GREY));
#if LV_FONT_MONTSERRAT_26
    lv_obj_set_style_text_font(eng_temp_meter, &lv_font_montserrat_26, LV_PART_TICKS);
#endif
    lv_meter_set_scale_major_ticks(eng_temp_meter, eng_temp_scale, 4, 2, 7, lv_palette_main(LV_PALETTE_GREY), 10);
    lv_meter_set_scale_range(eng_temp_meter, eng_temp_scale, 0, 120, 270, 90);
    eng_temp_indic = lv_meter_add_arc(eng_temp_meter, eng_temp_scale, 3, lv_palette_main(LV_PALETTE_ORANGE), 1);
    lv_meter_set_indicator_start_value(eng_temp_meter, eng_temp_indic, 0);

    lv_obj_t *eng_temp_label = lv_label_create(parent);
    lv_obj_align(eng_temp_label, LV_ALIGN_BOTTOM_RIGHT, -80, -2);
#if LV_FONT_MONTSERRAT_32
    lv_obj_set_style_text_font(eng_temp_label, &lv_font_montserrat_32, 0);
#endif
    lv_label_set_text_static(eng_temp_label, LV_SYMBOL_DEGREES "C");

    eng_sog_label = lv_label_create(parent);
    lv_obj_align(eng_sog_label, LV_ALIGN_TOP_LEFT, 2, 2);
#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(eng_sog_label, &lv_font_montserrat_30, 0);
#endif
    lv_label_set_text_static(eng_sog_label, "SOG (kt):\n--");

    eng_alternator_label = lv_label_create(parent);
    lv_obj_align(eng_alternator_label, LV_ALIGN_TOP_RIGHT, -2, 2);
#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(eng_alternator_label, &lv_font_montserrat_30, 0);
#endif
    lv_label_set_text_static(eng_alternator_label, "ALT (V):\n--");
}

// Update callback
void engine_update_cb() {
    set_engine_rpm_value(engine_rpm_indic,
        (fresh(shipDataModel.propulsion.engines[0].revolutions_RPM.age, 1)
            ? shipDataModel.propulsion.engines[0].revolutions_RPM.rpm / 100
            : 0));

    lv_meter_set_indicator_end_value(oil_press_meter, oil_press_indic,
        (fresh(shipDataModel.propulsion.engines[0].oil_pressure.age, 1)
            ? shipDataModel.propulsion.engines[0].oil_pressure.hPa * 0.0145037738
            : 0));

    lv_meter_set_indicator_end_value(eng_temp_meter, eng_temp_indic,
        (fresh(shipDataModel.propulsion.engines[0].temp_deg_C.age, 1)
            ? shipDataModel.propulsion.engines[0].temp_deg_C.deg_C
            : 0));

    lv_label_set_text(eng_sog_label,
        (String("SOG (kt):\n") +
         (fresh(shipDataModel.navigation.speed_over_ground.age)
            ? String(shipDataModel.navigation.speed_over_ground.kn, 1)
            : String("--"))).c_str());

    lv_label_set_text(eng_alternator_label,
        (String("ALT (V):\n    ") +
         (fresh(shipDataModel.propulsion.engines[0].alternator_voltage.age)
            ? String(shipDataModel.propulsion.engines[0].alternator_voltage.volt, 1)
            : String("--"))).c_str());
}

// Initialize screen struct
void init_engineScreen() {
    engineScreen.screen = lv_obj_create(nullptr);
    engineScreen.init_cb = lv_engine_display;
    engineScreen.update_cb = engine_update_cb;
}