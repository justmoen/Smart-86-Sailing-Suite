#include "ui_engine.h"
#include "ship_data_model.h"
#include "ship_data_util.h"
#include <ui_init.h>
#include "ui_screens.h"
#include "signalk_path_config.h"
#include <cstdint>
#include <cmath>

// Maximum number of engine screens we support
#define MAX_ENGINE_SCREENS 8

// -----------------------------------------------------------------------------
// LVGL 9 engine gauge implementation
// -----------------------------------------------------------------------------

// Per-engine screen state
struct EngineScreenState {
    int engine_id;

    // RPM scale & needle
    lv_obj_t *engine_rpm_scale;
    lv_obj_t *engine_rpm_needle;

    // Oil pressure scale & needle
    lv_obj_t *oil_press_scale;
    lv_obj_t *oil_press_needle;

    // Temperature scale & needle
    lv_obj_t *eng_temp_scale;
    lv_obj_t *eng_temp_needle;

    // Labels
    lv_obj_t *eng_sog_label;
    lv_obj_t *eng_alternator_label;
    lv_obj_t *rpm_value_label;
    lv_obj_t *engine_id_label;

    // Last known values
    uint32_t last_update_ms = 0;
    float last_rpm = 0;
    float last_oil_pressure = 0;
    float last_temp = 0;
    float last_alternator = 0;
    float last_battery_voltage = 0;
    float last_throttle = 0;
};

static EngineScreenState engine_states[MAX_ENGINE_SCREENS] = {};
static lv_updatable_screen_t engine_screens_array[MAX_ENGINE_SCREENS] = {};
static int engine_screens_created = 0;

// =============================================================================
// Helper Function for Configuring Section Ticks/Arc Styles in LVGL 9
// =============================================================================
static void add_gauge_section(lv_obj_t *scale, int32_t start, int32_t end, lv_color_t color)
{
    lv_scale_section_t *section = lv_scale_add_section(scale);
    lv_scale_section_set_range(section, start, end);
    
    // Allocate persistent custom styles for the colored items/ticks zone
    static lv_style_t section_part_style;
    static bool style_inited = false;
    if(!style_inited) {
        lv_style_init(&section_part_style);
        style_inited = true;
    }
    lv_style_set_line_color(&section_part_style, color);
    
    // Apply styling to LV_PART_ITEMS so the ticks change color in this section window
    lv_scale_section_set_style(section, LV_PART_ITEMS, &section_part_style);
}

// =============================================================================
// Display initialization
// =============================================================================
static void lv_engine_display(lv_updatable_screen_t *scr)
{
    EngineScreenState *state = (EngineScreenState *)scr->user_data;
    if (!state) return;

    const auto& config = get_signalk_path_config();

    // -------------------------------------------------------------------------
    // RPM GAUGE (Main Center Widget)
    // -------------------------------------------------------------------------
    state->engine_rpm_scale = lv_scale_create(scr->screen);
    lv_obj_set_size(state->engine_rpm_scale, 680, 680);
    lv_obj_center(state->engine_rpm_scale);
    
    // Enable visible solid grey background drawing for the scale face
    lv_obj_set_style_bg_color(state->engine_rpm_scale, lv_color_hex(0xD0D0D0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(state->engine_rpm_scale, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(state->engine_rpm_scale, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    
    // Configure Round Inner Layout
    lv_scale_set_mode(state->engine_rpm_scale, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_label_show(state->engine_rpm_scale, true);
    
    // Range maps 0..60 (x100 RPM)
    lv_scale_set_range(state->engine_rpm_scale, 0, 60);
    lv_scale_set_angle_range(state->engine_rpm_scale, 240);
    lv_scale_set_rotation(state->engine_rpm_scale, 150);
    
    // Distribute Ticks (61 marks total)
    lv_scale_set_total_tick_count(state->engine_rpm_scale, 61);
    lv_scale_set_major_tick_every(state->engine_rpm_scale, 10);
    
    // Ticks & Main Ring Colors
    lv_obj_set_style_arc_color(state->engine_rpm_scale, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_arc_width(state->engine_rpm_scale, 4, LV_PART_MAIN);
    lv_obj_set_style_line_color(state->engine_rpm_scale, lv_color_black(), LV_PART_ITEMS);
    
    // Colored Alert Sectors
    add_gauge_section(state->engine_rpm_scale, 0, 20, lv_palette_main(LV_PALETTE_BLUE));   // Blue operational range
    add_gauge_section(state->engine_rpm_scale, 40, 60, lv_palette_main(LV_PALETTE_RED));  // Danger redline zone

    // Create Needle Line 
    state->engine_rpm_needle = lv_line_create(state->engine_rpm_scale);
    
    // CRITICAL: Stop the scale layout engine from flattening the needle object down to 0x0 size
    lv_obj_set_layout(state->engine_rpm_needle, LV_LAYOUT_NONE);
    
    // Style Needle Line parameters
    lv_obj_set_style_line_width(state->engine_rpm_needle, 6, LV_PART_MAIN);
    lv_obj_set_style_line_color(state->engine_rpm_needle, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(state->engine_rpm_needle, true, LV_PART_MAIN);

    // Initial needle index point placement
    lv_scale_set_line_needle_value(state->engine_rpm_scale, state->engine_rpm_needle, 280, 0);

    // -------------------------------------------------------------------------
    // RPM CENTER LABEL
    // -------------------------------------------------------------------------
    lv_obj_t *main_label = lv_label_create(scr->screen);
    lv_obj_align(main_label, LV_ALIGN_CENTER, 0, 50);

#if LV_FONT_MONTSERRAT_22
    lv_obj_set_style_text_font(main_label, &lv_font_montserrat_22, LV_PART_MAIN);
#endif
    lv_obj_set_style_text_color(main_label, lv_color_black(), LV_PART_MAIN);
    lv_label_set_text_static(main_label, "RPMx100");

    // -------------------------------------------------------------------------
    // OIL PRESSURE GAUGE (Sub-dial Left)
    // -------------------------------------------------------------------------
    if (config.engine_oil_pressure_enabled) {
        state->oil_press_scale = lv_scale_create(scr->screen);
        lv_obj_set_size(state->oil_press_scale, 160, 160);
        lv_obj_align(state->oil_press_scale, LV_ALIGN_CENTER, -125, 180);
        
        // Background face config
        lv_obj_set_style_bg_color(state->oil_press_scale, lv_color_hex(0xD0D0D0), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(state->oil_press_scale, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(state->oil_press_scale, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        
        lv_scale_set_mode(state->oil_press_scale, LV_SCALE_MODE_ROUND_INNER);
        lv_scale_set_label_show(state->oil_press_scale, true);
        
        lv_scale_set_range(state->oil_press_scale, 0, 90);
        lv_scale_set_angle_range(state->oil_press_scale, 270);
        lv_scale_set_rotation(state->oil_press_scale, 90);
        
        lv_scale_set_total_tick_count(state->oil_press_scale, 10);
        lv_scale_set_major_tick_every(state->oil_press_scale, 3);

        lv_obj_set_style_arc_color(state->oil_press_scale, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_arc_width(state->oil_press_scale, 3, LV_PART_MAIN);

        // Sections configuration using proper multi-zone structures
        add_gauge_section(state->oil_press_scale, 0, config.engine_oil_pressure_min, lv_palette_main(LV_PALETTE_RED));
        add_gauge_section(state->oil_press_scale, config.engine_oil_pressure_min, config.engine_oil_pressure_max, lv_palette_main(LV_PALETTE_GREEN));
        add_gauge_section(state->oil_press_scale, config.engine_oil_pressure_max, 90, lv_palette_main(LV_PALETTE_RED));

        // Create Needle Line 
        state->oil_press_needle = lv_line_create(state->oil_press_scale);
        lv_obj_set_layout(state->oil_press_needle, LV_LAYOUT_NONE); // Prevents item layout collapse
        
        lv_obj_set_style_line_width(state->oil_press_needle, 4, LV_PART_MAIN);
        lv_obj_set_style_line_color(state->oil_press_needle, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
        lv_obj_set_style_line_rounded(state->oil_press_needle, true, LV_PART_MAIN);
        
        lv_scale_set_line_needle_value(state->oil_press_scale, state->oil_press_needle, 65, 0);

        // Units Text
        lv_obj_t *oil_press_label = lv_label_create(scr->screen);
        lv_obj_align(oil_press_label, LV_ALIGN_CENTER, -125, 280);

#if LV_FONT_MONTSERRAT_32
        lv_obj_set_style_text_font(oil_press_label, &lv_font_montserrat_32, LV_PART_MAIN);
#endif
        lv_obj_set_style_text_color(oil_press_label, lv_color_black(), LV_PART_MAIN);
        lv_label_set_text_static(oil_press_label, "psi");
    }

    // -------------------------------------------------------------------------
    // ENGINE TEMPERATURE GAUGE (Sub-dial Right)
    // -------------------------------------------------------------------------
    state->eng_temp_scale = lv_scale_create(scr->screen);
    lv_obj_set_size(state->eng_temp_scale, 160, 160);

    int temp_x = config.engine_oil_pressure_enabled ? 125 : 0;
    int temp_y = 190;
    lv_obj_align(state->eng_temp_scale, LV_ALIGN_CENTER, temp_x, temp_y);
    
    // Background face color parameters
    lv_obj_set_style_bg_color(state->eng_temp_scale, lv_color_hex(0xD0D0D0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(state->eng_temp_scale, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(state->eng_temp_scale, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    
    lv_scale_set_mode(state->eng_temp_scale, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_label_show(state->eng_temp_scale, true);
    
    lv_scale_set_range(state->eng_temp_scale, 0, 120);
    lv_scale_set_angle_range(state->eng_temp_scale, 270);
    lv_scale_set_rotation(state->eng_temp_scale, 90);
    
    lv_scale_set_total_tick_count(state->eng_temp_scale, 13);
    lv_scale_set_major_tick_every(state->eng_temp_scale, 4);

    lv_obj_set_style_arc_color(state->eng_temp_scale, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_arc_width(state->eng_temp_scale, 3, LV_PART_MAIN);

    // Dynamic temperature sections
    add_gauge_section(state->eng_temp_scale, 0, config.engine_temp_redline, lv_palette_main(LV_PALETTE_GREEN));
    add_gauge_section(state->eng_temp_scale, config.engine_temp_redline, 120, lv_palette_main(LV_PALETTE_RED));

    // Needle Line 
    state->eng_temp_needle = lv_line_create(state->eng_temp_scale);
    lv_obj_set_layout(state->eng_temp_needle, LV_LAYOUT_NONE); // Disables conflicting grid calculations
    
    lv_obj_set_style_line_width(state->eng_temp_needle, 4, LV_PART_MAIN);
    lv_obj_set_style_line_color(state->eng_temp_needle, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(state->eng_temp_needle, true, LV_PART_MAIN);
    
    lv_scale_set_line_needle_value(state->eng_temp_scale, state->eng_temp_needle, 65, 0);

    // Temperature text units
    lv_obj_t *eng_temp_label = lv_label_create(scr->screen);
    lv_obj_align(eng_temp_label, LV_ALIGN_CENTER, temp_x, 290);
    lv_obj_set_style_text_font(eng_temp_label, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(eng_temp_label, lv_color_black(), LV_PART_MAIN);
    lv_label_set_text_static(eng_temp_label, LV_SYMBOL_DEGREES "C");

    // -------------------------------------------------------------------------
    // TOP LEFT INFORMATION
    // -------------------------------------------------------------------------
    state->eng_sog_label = lv_label_create(scr->screen);
    lv_obj_align(state->eng_sog_label, LV_ALIGN_TOP_LEFT, 2, 2);
    lv_obj_set_style_text_font(state->eng_sog_label, &lv_font_montserrat_30, LV_PART_MAIN);

    if (config.engine_top_left_enabled) {
        if (config.engine_top_left_metric == EngineTopLeftMetric::SOG) {
            lv_label_set_text_static(state->eng_sog_label, "SOG (kt):--");
        } else {
            lv_label_set_text_static(state->eng_sog_label, "THROTTLE (%):--");
        }
    } else {
        lv_obj_add_flag(state->eng_sog_label, LV_OBJ_FLAG_HIDDEN);
    }

    // -------------------------------------------------------------------------
    // TOP RIGHT INFORMATION
    // -------------------------------------------------------------------------
    state->eng_alternator_label = lv_label_create(scr->screen);
    lv_obj_align(state->eng_alternator_label, LV_ALIGN_TOP_RIGHT, -2, 2);
    lv_obj_set_style_text_font(state->eng_alternator_label, &lv_font_montserrat_30, LV_PART_MAIN);

    if (config.engine_top_right_enabled) {
        if (config.engine_top_right_metric == EngineTopRightMetric::AlternatorVoltage) {
            lv_label_set_text_static(state->eng_alternator_label, "ALT (V):--");
        } else {
            lv_label_set_text_static(state->eng_alternator_label, "BAT (V):--");
        }
    } else {
        lv_obj_add_flag(state->eng_alternator_label, LV_OBJ_FLAG_HIDDEN);
    }

    // -------------------------------------------------------------------------
    // RPM NUMERIC VALUE
    // -------------------------------------------------------------------------
    state->rpm_value_label = lv_label_create(scr->screen);
    lv_obj_align(state->rpm_value_label, LV_ALIGN_BOTTOM_LEFT, 8, -6);
    lv_obj_set_style_text_font(state->rpm_value_label, &lv_font_montserrat_26, LV_PART_MAIN);
    lv_obj_set_style_text_color(state->rpm_value_label, lv_color_black(), LV_PART_MAIN);
    lv_label_set_text_static(state->rpm_value_label, "RPM: --");

    // -------------------------------------------------------------------------
    // ENGINE ID
    // -------------------------------------------------------------------------
    state->engine_id_label = lv_label_create(scr->screen);
    lv_obj_align(state->engine_id_label, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
    lv_obj_set_style_text_font(state->engine_id_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(state->engine_id_label, lv_color_hex(0x999999), LV_PART_MAIN);

    String engine_id_str = String("E") + String(state->engine_id + 1);
    lv_label_set_text(state->engine_id_label, engine_id_str.c_str());
}

// =============================================================================
// Update callback
// =============================================================================
static void engine_update_cb(lv_updatable_screen_t *scr)
{
    EngineScreenState *state = (EngineScreenState *)scr->user_data;
    if (!state || !scr->screen) return;

    const uint32_t now = millis();
    if ((now - state->last_update_ms) < 1000) {
        return;
    }
    state->last_update_ms = now;

    const auto& config = get_signalk_path_config();

    if (!state->engine_rpm_needle || !state->eng_temp_needle ||
        !state->eng_sog_label || !state->eng_alternator_label || !state->rpm_value_label) {
        return;
    }

    if (config.engine_oil_pressure_enabled && !state->oil_press_needle) {
        return;
    }

    int engine_id = state->engine_id;
    if (engine_id < 0)  engine_id = 0;
    if (engine_id >= 8) engine_id = 7;

    // -------------------------------------------------------------------------
    // RPM Update
    // -------------------------------------------------------------------------
    if (fresh(shipDataModel.propulsion.engines[engine_id].revolutions_RPM.age)) {
        state->last_rpm = shipDataModel.propulsion.engines[engine_id].revolutions_RPM.rpm;
    }

    float scaled_rpm = state->last_rpm;
    if (scaled_rpm < 0.0f)    scaled_rpm = 0.0f;
    if (scaled_rpm > 6000.0f) scaled_rpm = 6000.0f;

    float rpm_gauge_value = scaled_rpm / 100.0f;

    lv_scale_set_line_needle_value(state->engine_rpm_scale, state->engine_rpm_needle, 280, rpm_gauge_value);

    lv_label_set_text(
        state->rpm_value_label,
        (String("RPM: ") + String((int32_t)lroundf(state->last_rpm))).c_str()
    );

    // -------------------------------------------------------------------------
    // TOP LEFT Update
    // -------------------------------------------------------------------------
    if (config.engine_top_left_enabled) {
        if (config.engine_top_left_metric == EngineTopLeftMetric::SOG) {
            float sog = shipDataModel.navigation.speed_over_ground.kn;
            lv_label_set_text(state->eng_sog_label, (String("SOG (kt):") + String(sog, 1)).c_str());
        } else {
            if (fresh(shipDataModel.propulsion.engines[engine_id].throttle.age)) {
                state->last_throttle = shipDataModel.propulsion.engines[engine_id].throttle.pct;
            }
            lv_label_set_text(state->eng_sog_label, (String("THROTTLE (%):") + String(state->last_throttle, 0)).c_str());
        }
    }

    // -------------------------------------------------------------------------
    // TOP RIGHT Update
    // -------------------------------------------------------------------------
    if (config.engine_top_right_enabled) {
        if (config.engine_top_right_metric == EngineTopRightMetric::AlternatorVoltage) {
            if (fresh(shipDataModel.propulsion.engines[engine_id].alternator_voltage.age)) {
                state->last_alternator = shipDataModel.propulsion.engines[engine_id].alternator_voltage.volt;
            }
            lv_label_set_text(state->eng_alternator_label, (String("ALT (V):") + String(state->last_alternator, 1)).c_str());
        } else {
            if (fresh(shipDataModel.propulsion.engines[engine_id].battery_voltage.age)) {
                state->last_battery_voltage = shipDataModel.propulsion.engines[engine_id].battery_voltage.volt;
            }
            lv_label_set_text(state->eng_alternator_label, (String("BAT (V):") + String(state->last_battery_voltage, 1)).c_str());
        }
    }

    // -------------------------------------------------------------------------
    // OIL PRESSURE Update
    // -------------------------------------------------------------------------
    if (config.engine_oil_pressure_enabled) {
        if (fresh(shipDataModel.propulsion.engines[engine_id].oil_pressure.age)) {
            state->last_oil_pressure = shipDataModel.propulsion.engines[engine_id].oil_pressure.hPa * 0.0145037738f;
        }

        if (state->last_oil_pressure < 0.0f)  state->last_oil_pressure = 0.0f;
        if (state->last_oil_pressure > 90.0f) state->last_oil_pressure = 90.0f;

        lv_scale_set_line_needle_value(state->oil_press_scale, state->oil_press_needle, 65, state->last_oil_pressure);
    }

    // -------------------------------------------------------------------------
    // ENGINE TEMPERATURE Update
    // -------------------------------------------------------------------------
    if (fresh(shipDataModel.propulsion.engines[engine_id].temp_deg_C.age)) {
        state->last_temp = shipDataModel.propulsion.engines[engine_id].temp_deg_C.deg_C;
    }

    if (state->last_temp < 0.0f)   state->last_temp = 0.0f;
    if (state->last_temp > 120.0f) state->last_temp = 120.0f;

    lv_scale_set_line_needle_value(state->eng_temp_scale, state->eng_temp_needle, 65, state->last_temp);
}

// =============================================================================
// Factory function
// =============================================================================
void create_engine_screens(lv_updatable_screen_t **out_screens, int *out_count)
{
    const auto& config = get_signalk_path_config();
    int num_engines = config.num_engines;

    if (num_engines < 1)                  num_engines = 1;
    if (num_engines > MAX_ENGINE_SCREENS) num_engines = MAX_ENGINE_SCREENS;

    for (int i = 0; i < num_engines; i++) {
        int engine_id = 0;
        if (i == 0)      engine_id = config.engine_screen_1_id;
        else if (i == 1) engine_id = config.engine_screen_2_id;
        else             engine_id = i;

        if (engine_id < 0)  engine_id = 0;
        if (engine_id >= 8) engine_id = 7;

        // Initialize state
        engine_states[i] = {};
        engine_states[i].engine_id = engine_id;

        // Initialize screen
        engine_screens_array[i].screen    = nullptr;
        engine_screens_array[i].created   = false;
        engine_screens_array[i].create_cb  = lv_engine_display;
        engine_screens_array[i].update_cb  = engine_update_cb;
        engine_screens_array[i].user_data  = &engine_states[i];

        out_screens[i] = &engine_screens_array[i];
    }

    *out_count = num_engines;
    engine_screens_created = num_engines;
}

// =============================================================================
// Get number of engine screens
// =============================================================================
int get_engine_screen_count()
{
    return engine_screens_created;
}

// =============================================================================
// Legacy single engine screen
// =============================================================================
lv_updatable_screen_t engineScreen = {
    .screen = nullptr,
    .created = false,
    .create_cb = nullptr,
    .update_cb = nullptr,
    .user_data = nullptr
};
