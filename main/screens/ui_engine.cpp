#include "ui_engine.h"
#include "ship_data_model.h"
#include "ship_data_util.h"
#include <ui_init.h>
#include "ui_screens.h"
#include "signalk_path_config.h"
#include <cstdint>

// Maximum number of engine screens we support
#define MAX_ENGINE_SCREENS 8

// Per-engine screen state
struct EngineScreenState {
    int engine_id;
    lv_obj_t *engine_rpm_meter;
    lv_meter_indicator_t *engine_rpm_indic;
    lv_obj_t *oil_press_meter;
    lv_meter_indicator_t *oil_press_indic;
    lv_obj_t *eng_temp_meter;
    lv_meter_indicator_t *temp_arc_green;
    lv_meter_indicator_t *temp_arc_red;
    lv_meter_indicator_t *temp_needle;
    lv_obj_t *eng_sog_label;
    lv_obj_t *eng_alternator_label;
    float last_rpm = 0;
    float last_oil_pressure = 0;
    float last_temp = 0;
    float last_alternator = 0;
};

static EngineScreenState engine_states[MAX_ENGINE_SCREENS] = {};
static lv_updatable_screen_t engine_screens_array[MAX_ENGINE_SCREENS] = {};
static int engine_screens_created = 0;

// Helper to set RPM for a specific engine screen
static void set_engine_rpm_value(EngineScreenState *state, int32_t v) {
    if (state && state->engine_rpm_meter) {
        lv_meter_set_indicator_value(state->engine_rpm_meter, state->engine_rpm_indic, v);
    }
}

// Display initialization - parameterized by engine_id
static void lv_engine_display(lv_updatable_screen_t *scr) {
    EngineScreenState *state = (EngineScreenState *)scr->user_data;
    if (!state) return;

    const auto& config = get_signalk_path_config();
    
    state->engine_rpm_meter = lv_meter_create(scr->screen);
    apply_meter_style(state->engine_rpm_meter);
    lv_obj_center(state->engine_rpm_meter);
    lv_obj_set_size(state->engine_rpm_meter, 680, 680);

    lv_obj_t *main_label = lv_label_create(scr->screen);
    lv_obj_align(main_label, LV_ALIGN_CENTER, 0, 50);
#if LV_FONT_MONTSERRAT_22
    lv_obj_set_style_text_font(main_label, &lv_font_montserrat_22, 0);
#endif
    lv_obj_set_style_text_color(main_label, lv_color_black(), 0);
    String rpm_label = String("RPM\nx100");
    lv_label_set_text(main_label, rpm_label.c_str());

    // Add scale
    lv_meter_scale_t *scale = lv_meter_add_scale(state->engine_rpm_meter);
    lv_meter_set_scale_ticks(state->engine_rpm_meter, scale, 31, 2, 10, lv_palette_main(LV_PALETTE_GREY));
#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(state->engine_rpm_meter, &lv_font_montserrat_30, LV_PART_TICKS);
#endif
    lv_meter_set_scale_major_ticks(state->engine_rpm_meter, scale, 5, 4, 15, lv_palette_main(LV_PALETTE_GREY), 20);
    lv_meter_set_scale_range(state->engine_rpm_meter, scale, 0, 60, 240, 150);

    // Blue arc and ticks
    state->engine_rpm_indic = lv_meter_add_arc(state->engine_rpm_meter, scale, 3, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_meter_set_indicator_start_value(state->engine_rpm_meter, state->engine_rpm_indic, 0);
    lv_meter_set_indicator_end_value(state->engine_rpm_meter, state->engine_rpm_indic, 20);
    state->engine_rpm_indic = lv_meter_add_scale_lines(
        state->engine_rpm_meter, scale, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_BLUE), true, 0);
    lv_meter_set_indicator_start_value(state->engine_rpm_meter, state->engine_rpm_indic, 0);
    lv_meter_set_indicator_end_value(state->engine_rpm_meter, state->engine_rpm_indic, 20);

    // Red arc and ticks
    state->engine_rpm_indic = lv_meter_add_arc(state->engine_rpm_meter, scale, 3, lv_palette_main(LV_PALETTE_RED), 0);
    lv_meter_set_indicator_start_value(state->engine_rpm_meter, state->engine_rpm_indic, 40);
    lv_meter_set_indicator_end_value(state->engine_rpm_meter, state->engine_rpm_indic, 60);
    state->engine_rpm_indic = lv_meter_add_scale_lines(
        state->engine_rpm_meter, scale, lv_palette_main(LV_PALETTE_RED), lv_palette_main(LV_PALETTE_RED), true, 0);
    lv_meter_set_indicator_start_value(state->engine_rpm_meter, state->engine_rpm_indic, 40);
    lv_meter_set_indicator_end_value(state->engine_rpm_meter, state->engine_rpm_indic, 60);

    // Needle
    state->engine_rpm_indic = lv_meter_add_needle_line(state->engine_rpm_meter, scale, 4, lv_palette_main(LV_PALETTE_GREY), -10);

    // Oil pressure
    state->oil_press_meter = lv_meter_create(scr->screen);
    lv_obj_align(state->oil_press_meter, LV_ALIGN_CENTER, -125, 180);
    lv_obj_set_size(state->oil_press_meter, 160, 160);
    lv_obj_remove_style(state->oil_press_meter, NULL, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(state->oil_press_meter, 0, LV_PART_MAIN);
    lv_meter_scale_t *oil_press_scale = lv_meter_add_scale(state->oil_press_meter);
    lv_meter_set_scale_ticks(state->oil_press_meter, oil_press_scale, 10, 2, 7, lv_palette_main(LV_PALETTE_GREY));
#if LV_FONT_MONTSERRAT_26
    lv_obj_set_style_text_font(state->oil_press_meter, &lv_font_montserrat_26, LV_PART_TICKS);
#endif
    lv_meter_set_scale_major_ticks(state->oil_press_meter, oil_press_scale, 3, 2, 7, lv_palette_main(LV_PALETTE_GREY), 10);
    lv_meter_set_scale_range(state->oil_press_meter, oil_press_scale, 0, 90, 270, 90);
    
    float oil_min_psi = config.engine_oil_pressure_min;
    float oil_max_psi = config.engine_oil_pressure_max;
    
    // Green zone arc
    state->oil_press_indic = lv_meter_add_arc(state->oil_press_meter, oil_press_scale, 3, lv_palette_main(LV_PALETTE_GREEN), 1);
    lv_meter_set_indicator_start_value(state->oil_press_meter, state->oil_press_indic, oil_min_psi);
    lv_meter_set_indicator_end_value(state->oil_press_meter, state->oil_press_indic, oil_max_psi);
    
    // Red zones (below min and above max)
    lv_meter_indicator_t *red_low = lv_meter_add_arc(state->oil_press_meter, oil_press_scale, 3, lv_palette_main(LV_PALETTE_RED), 1);
    lv_meter_set_indicator_start_value(state->oil_press_meter, red_low, 0);
    lv_meter_set_indicator_end_value(state->oil_press_meter, red_low, oil_min_psi);
    
    lv_meter_indicator_t *red_high = lv_meter_add_arc(state->oil_press_meter, oil_press_scale, 3, lv_palette_main(LV_PALETTE_RED), 1);
    lv_meter_set_indicator_start_value(state->oil_press_meter, red_high, oil_max_psi);
    lv_meter_set_indicator_end_value(state->oil_press_meter, red_high, 90);

    lv_obj_t *oil_press_label = lv_label_create(scr->screen);
    lv_obj_align(oil_press_label, LV_ALIGN_CENTER, -125, 280);
#if LV_FONT_MONTSERRAT_32
    lv_obj_set_style_text_font(oil_press_label, &lv_font_montserrat_32, 0);
#endif
    lv_obj_set_style_text_color(oil_press_label, lv_color_black(), 0);
    lv_label_set_text_static(oil_press_label, "psi");

    // Engine temp
    state->eng_temp_meter = lv_meter_create(scr->screen);
    lv_obj_align(state->eng_temp_meter, LV_ALIGN_CENTER, 125, 180);
    lv_obj_set_size(state->eng_temp_meter, 160, 160);
    lv_obj_remove_style(state->eng_temp_meter, NULL, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(state->eng_temp_meter, 0, LV_PART_MAIN);
    lv_meter_scale_t *eng_temp_scale = lv_meter_add_scale(state->eng_temp_meter);
    lv_meter_set_scale_ticks(state->eng_temp_meter, eng_temp_scale, 13, 2, 7, lv_palette_main(LV_PALETTE_GREY));
    lv_obj_set_style_text_font(state->eng_temp_meter, &lv_font_montserrat_26, LV_PART_TICKS);
    lv_meter_set_scale_major_ticks(state->eng_temp_meter, eng_temp_scale, 4, 2, 7, lv_palette_main(LV_PALETTE_GREY), 10);
    lv_meter_set_scale_range(state->eng_temp_meter, eng_temp_scale, 0, 120, 270, 90);

    // Green zone (0 to redline)
    state->temp_arc_green = lv_meter_add_arc(state->eng_temp_meter, eng_temp_scale, 3, lv_palette_main(LV_PALETTE_GREEN), 1);
    lv_meter_set_indicator_start_value(state->eng_temp_meter, state->temp_arc_green, 0);
    lv_meter_set_indicator_end_value(state->eng_temp_meter, state->temp_arc_green, config.engine_temp_redline);
    
    // Red zone (redline to max)
    state->temp_arc_red = lv_meter_add_arc(state->eng_temp_meter, eng_temp_scale, 3, lv_palette_main(LV_PALETTE_RED), 1);
    lv_meter_set_indicator_start_value(state->eng_temp_meter, state->temp_arc_red, config.engine_temp_redline);
    lv_meter_set_indicator_end_value(state->eng_temp_meter, state->temp_arc_red, 120);

    state->temp_needle = lv_meter_add_needle_line(state->eng_temp_meter, eng_temp_scale, 3, lv_palette_main(LV_PALETTE_GREY), -10);

    lv_obj_t *eng_temp_label = lv_label_create(scr->screen);
    lv_obj_align(eng_temp_label, LV_ALIGN_CENTER, 125, 280);
    lv_obj_set_style_text_font(eng_temp_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(eng_temp_label, lv_color_black(), 0);
    lv_label_set_text_static(eng_temp_label, LV_SYMBOL_DEGREES "C");

    state->eng_sog_label = lv_label_create(scr->screen);
    lv_obj_align(state->eng_sog_label, LV_ALIGN_TOP_LEFT, 2, 2);
    lv_obj_set_style_text_font(state->eng_sog_label, &lv_font_montserrat_30, 0);
    lv_label_set_text_static(state->eng_sog_label, "SOG (kt):\n--");

    state->eng_alternator_label = lv_label_create(scr->screen);
    lv_obj_align(state->eng_alternator_label, LV_ALIGN_TOP_RIGHT, -2, 2);
    lv_obj_set_style_text_font(state->eng_alternator_label, &lv_font_montserrat_30, 0);
    lv_label_set_text_static(state->eng_alternator_label, "ALT (V):\n--");
    
    // Display engine ID in bottom right corner
    lv_obj_t *engine_id_label = lv_label_create(scr->screen);
    lv_obj_align(engine_id_label, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
    lv_obj_set_style_text_font(engine_id_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(engine_id_label, lv_color_hex(0x999999), 0);
    String engine_id_str = String("E") + (state->engine_id + 1);
    lv_label_set_text(engine_id_label, engine_id_str.c_str());
}

// Update callback - parameterized by engine_id
static void engine_update_cb(lv_updatable_screen_t *scr) {
    EngineScreenState *state = (EngineScreenState *)scr->user_data;
    if (!state || !scr->screen) return;

    if (!state->engine_rpm_indic || !state->oil_press_indic || !state->temp_arc_green || !state->temp_arc_red || !state->temp_needle || 
        !state->eng_sog_label || !state->eng_alternator_label) return;
    
    int engine_id = state->engine_id;
    if (engine_id < 0) engine_id = 0;
    if (engine_id >= 8) engine_id = 7;
    
    // Use configured engine_id
    if (engine_id >= 0 && engine_id < 8) {
        if (fresh(shipDataModel.propulsion.engines[engine_id].revolutions_RPM.age)) {
            state->last_rpm = shipDataModel.propulsion.engines[engine_id].revolutions_RPM.rpm / 100;
        }
        set_engine_rpm_value(state, state->last_rpm);

        if (fresh(shipDataModel.propulsion.engines[engine_id].alternator_voltage.age)) {
            state->last_alternator =
                shipDataModel.propulsion.engines[engine_id].alternator_voltage.volt;
        }

        lv_label_set_text(state->eng_alternator_label,
            (String("ALT (V):\n    ") + String(state->last_alternator, 1)).c_str());
        
        if (fresh(shipDataModel.propulsion.engines[engine_id].oil_pressure.age)) {
            state->last_oil_pressure =
                shipDataModel.propulsion.engines[engine_id].oil_pressure.hPa * 0.0145037738;
        }

        lv_meter_set_indicator_end_value(
            state->oil_press_meter,
            state->oil_press_indic,
            state->last_oil_pressure
        );

        if (fresh(shipDataModel.propulsion.engines[engine_id].temp_deg_C.age)) {
            state->last_temp = shipDataModel.propulsion.engines[engine_id].temp_deg_C.deg_C;
        }

        lv_meter_set_indicator_value(state->eng_temp_meter, state->temp_needle, state->last_temp);
    }
}

// Factory function to create N engine screens based on num_engines config
void create_engine_screens(lv_updatable_screen_t **out_screens, int *out_count) {
    const auto& config = get_signalk_path_config();
    int num_engines = config.num_engines;
    if (num_engines < 1) num_engines = 1;
    if (num_engines > MAX_ENGINE_SCREENS) num_engines = MAX_ENGINE_SCREENS;
    
    for (int i = 0; i < num_engines; i++) {
        // Get engine_id from config
        int engine_id = 0;
        if (i == 0) engine_id = config.engine_screen_1_id;
        else if (i == 1) engine_id = config.engine_screen_2_id;
        // For additional engines, just use the index
        else engine_id = i;
        
        if (engine_id < 0) engine_id = 0;
        if (engine_id >= 8) engine_id = 7;
        
        // Initialize state
        engine_states[i].engine_id = engine_id;
        engine_states[i].engine_rpm_meter = nullptr;
        engine_states[i].engine_rpm_indic = nullptr;
        engine_states[i].oil_press_meter = nullptr;
        engine_states[i].oil_press_indic = nullptr;
        engine_states[i].eng_temp_meter = nullptr;
        engine_states[i].temp_arc_green = nullptr;
        engine_states[i].temp_arc_red = nullptr;
        engine_states[i].temp_needle = nullptr;
        engine_states[i].eng_sog_label = nullptr;
        engine_states[i].eng_alternator_label = nullptr;
        
        // Create screen with callbacks and user_data pointing to state
        engine_screens_array[i].screen = nullptr;
        engine_screens_array[i].created = false;
        engine_screens_array[i].create_cb = lv_engine_display;
        engine_screens_array[i].update_cb = engine_update_cb;
        engine_screens_array[i].user_data = &engine_states[i];
        
        out_screens[i] = &engine_screens_array[i];
    }
    
    *out_count = num_engines;
    engine_screens_created = num_engines;
}

// Get number of engine screens created
int get_engine_screen_count() {
    return engine_screens_created;
}

// Legacy single engine screen for backwards compatibility
lv_updatable_screen_t engineScreen = {
    .screen = nullptr,
    .created = false,
    .create_cb = nullptr,
    .update_cb = nullptr,
    .user_data = nullptr
};
