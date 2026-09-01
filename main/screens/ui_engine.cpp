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
//
// LVGL 8 used lv_meter for these displays.
// LVGL 9 removed lv_meter, so the gauges are implemented using lv_arc,
// labels, and a small amount of geometry instead.
// -----------------------------------------------------------------------------

// Per-engine screen state
struct EngineScreenState {
    int engine_id;

    // RPM gauge
    lv_obj_t *engine_rpm_meter;
    lv_obj_t *engine_rpm_arc;
    lv_obj_t *engine_rpm_needle;

    // Oil pressure gauge
    lv_obj_t *oil_press_meter;
    lv_obj_t *oil_press_arc;
    lv_obj_t *oil_press_needle;

    // Temperature gauge
    lv_obj_t *eng_temp_meter;
    lv_obj_t *eng_temp_arc;
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
// Utility functions
// =============================================================================

static void set_arc_value(lv_obj_t *arc, float value)
{
    if (!arc) return;

    lv_arc_set_value(
        arc,
        (int32_t)lroundf(value)
    );
}


// Convert a value in a gauge range to an LVGL arc angle.
//
// The gauges use the same approximate orientation as the old lv_meter
// implementation: a partial circular gauge with the bottom area open.
static int value_to_angle(
    float value,
    float min_value,
    float max_value,
    int start_angle,
    int end_angle)
{
    if (value < min_value) value = min_value;
    if (value > max_value) value = max_value;

    float fraction =
        (value - min_value) /
        (max_value - min_value);

    return start_angle +
           (int)lroundf(
               fraction *
               (float)(end_angle - start_angle)
           );
}


// =============================================================================
// Gauge creation helpers
// =============================================================================

static lv_obj_t *create_gauge_arc(
    lv_obj_t *parent,
    int32_t min_value,
    int32_t max_value,
    int start_angle,
    int end_angle,
    int size)
{
    lv_obj_t *arc = lv_arc_create(parent);

    lv_obj_set_size(arc, size, size);
    lv_obj_center(arc);

    lv_arc_set_range(
        arc,
        min_value,
        max_value
    );

    lv_arc_set_value(
        arc,
        min_value
    );

    lv_arc_set_bg_angles(
        arc,
        start_angle,
        end_angle
    );

    lv_arc_set_angles(
        arc,
        start_angle,
        start_angle
    );

    // Remove the knob.
    lv_obj_remove_style(
        arc,
        NULL,
        LV_PART_KNOB
    );

    // Background arc
    lv_obj_set_style_arc_width(
        arc,
        4,
        LV_PART_MAIN
    );

    lv_obj_set_style_arc_color(
        arc,
        lv_palette_main(LV_PALETTE_GREY),
        LV_PART_MAIN
    );

    // Indicator arc
    lv_obj_set_style_arc_width(
        arc,
        5,
        LV_PART_INDICATOR
    );

    lv_obj_set_style_arc_color(
        arc,
        lv_palette_main(LV_PALETTE_GREY),
        LV_PART_INDICATOR
    );

    return arc;
}


// Create a colored zone as a second arc.
//
// This is deliberately independent of the value indicator. It gives us the
// same concept as the old lv_meter green/red arcs.
static lv_obj_t *create_zone_arc(
    lv_obj_t *parent,
    int32_t min_value,
    int32_t max_value,
    int32_t zone_start,
    int32_t zone_end,
    int start_angle,
    int end_angle,
    int size,
    lv_color_t color)
{
    lv_obj_t *arc = lv_arc_create(parent);

    lv_obj_set_size(arc, size, size);
    lv_obj_center(arc);

    lv_arc_set_range(
        arc,
        min_value,
        max_value
    );

    lv_arc_set_bg_angles(
        arc,
        start_angle,
        end_angle
    );

    int zone_start_angle =
        value_to_angle(
            zone_start,
            min_value,
            max_value,
            start_angle,
            end_angle
        );

    int zone_end_angle =
        value_to_angle(
            zone_end,
            min_value,
            max_value,
            start_angle,
            end_angle
        );

    lv_arc_set_angles(
        arc,
        zone_start_angle,
        zone_end_angle
    );

    lv_obj_remove_style(
        arc,
        NULL,
        LV_PART_KNOB
    );

    lv_obj_set_style_arc_width(
        arc,
        5,
        LV_PART_INDICATOR
    );

    lv_obj_set_style_arc_color(
        arc,
        color,
        LV_PART_INDICATOR
    );

    // Make the background invisible.
    lv_obj_set_style_arc_opa(
        arc,
        LV_OPA_TRANSP,
        LV_PART_MAIN
    );

    return arc;
}


// Create a simple gauge needle.
//
// LVGL 9 doesn't have the old lv_meter needle indicator. A thin arc gives
// us a reliable replacement without introducing custom draw code.
static lv_obj_t *create_needle(
    lv_obj_t *parent,
    int32_t min_value,
    int32_t max_value,
    int start_angle,
    int end_angle,
    int size)
{
    lv_obj_t *needle = lv_arc_create(parent);

    lv_obj_set_size(
        needle,
        size,
        size
    );

    lv_obj_center(needle);

    lv_arc_set_range(
        needle,
        min_value,
        max_value
    );

    lv_arc_set_bg_angles(
        needle,
        start_angle,
        end_angle
    );

    lv_arc_set_angles(
        needle,
        start_angle,
        start_angle
    );

    lv_obj_remove_style(
        needle,
        NULL,
        LV_PART_KNOB
    );

    // Hide the background.
    lv_obj_set_style_arc_opa(
        needle,
        LV_OPA_TRANSP,
        LV_PART_MAIN
    );

    // Needle/indicator.
    lv_obj_set_style_arc_width(
        needle,
        6,
        LV_PART_INDICATOR
    );

    lv_obj_set_style_arc_color(
        needle,
        lv_palette_main(LV_PALETTE_GREY),
        LV_PART_INDICATOR
    );

    return needle;
}


static void set_needle_value(
    lv_obj_t *needle,
    float value,
    float min_value,
    float max_value,
    int start_angle,
    int end_angle)
{
    if (!needle) return;

    int angle =
        value_to_angle(
            value,
            min_value,
            max_value,
            start_angle,
            end_angle
        );

    lv_arc_set_angles(
        needle,
        start_angle,
        angle
    );
}


// =============================================================================
// Display initialization
// =============================================================================

static void lv_engine_display(lv_updatable_screen_t *scr)
{
    EngineScreenState *state =
        (EngineScreenState *)scr->user_data;

    if (!state) return;

    const auto& config =
        get_signalk_path_config();


    // -------------------------------------------------------------------------
    // RPM GAUGE
    // -------------------------------------------------------------------------

    state->engine_rpm_meter =
        lv_obj_create(scr->screen);

    lv_obj_remove_style(
        state->engine_rpm_meter,
        NULL,
        LV_PART_MAIN
    );

    lv_obj_set_size(
        state->engine_rpm_meter,
        680,
        680
    );

    lv_obj_center(
        state->engine_rpm_meter
    );

    lv_obj_set_style_bg_opa(
        state->engine_rpm_meter,
        LV_OPA_TRANSP,
        LV_PART_MAIN
    );


    // RPM gauge range = 0..60 (x100 RPM)
    const int rpm_start_angle = 150;
    const int rpm_end_angle   = 390;

    // Base gauge.
    state->engine_rpm_arc =
        create_gauge_arc(
            state->engine_rpm_meter,
            0,
            60,
            rpm_start_angle,
            rpm_end_angle,
            680
        );


    // Blue operating zone.
    create_zone_arc(
        state->engine_rpm_meter,
        0,
        60,
        0,
        20,
        rpm_start_angle,
        rpm_end_angle,
        680,
        lv_palette_main(LV_PALETTE_BLUE)
    );


    // Red operating zone.
    create_zone_arc(
        state->engine_rpm_meter,
        0,
        60,
        40,
        60,
        rpm_start_angle,
        rpm_end_angle,
        680,
        lv_palette_main(LV_PALETTE_RED)
    );


    // Needle.
    state->engine_rpm_needle =
        create_needle(
            state->engine_rpm_meter,
            0,
            60,
            rpm_start_angle,
            rpm_end_angle,
            680
        );


    // -------------------------------------------------------------------------
    // RPM CENTER LABEL
    // -------------------------------------------------------------------------

    lv_obj_t *main_label =
        lv_label_create(scr->screen);

    lv_obj_align(
        main_label,
        LV_ALIGN_CENTER,
        0,
        50
    );

#if LV_FONT_MONTSERRAT_22
    lv_obj_set_style_text_font(
        main_label,
        &lv_font_montserrat_22,
        LV_PART_MAIN
    );
#endif

    lv_obj_set_style_text_color(
        main_label,
        lv_color_black(),
        LV_PART_MAIN
    );

    lv_label_set_text_static(
        main_label,
        "RPM\nx100"
    );


    // -------------------------------------------------------------------------
    // OIL PRESSURE
    // -------------------------------------------------------------------------

    if (config.engine_oil_pressure_enabled) {

        state->oil_press_meter =
            lv_obj_create(scr->screen);

        lv_obj_remove_style(
            state->oil_press_meter,
            NULL,
            LV_PART_MAIN
        );

        lv_obj_set_size(
            state->oil_press_meter,
            160,
            160
        );

        lv_obj_align(
            state->oil_press_meter,
            LV_ALIGN_CENTER,
            -125,
            180
        );


        const int oil_start_angle = 90;
        const int oil_end_angle   = 360;


        // Base arc.
        state->oil_press_arc =
            create_gauge_arc(
                state->oil_press_meter,
                0,
                90,
                oil_start_angle,
                oil_end_angle,
                160
            );


        // Green operating zone.
        create_zone_arc(
            state->oil_press_meter,
            0,
            90,
            config.engine_oil_pressure_min,
            config.engine_oil_pressure_max,
            oil_start_angle,
            oil_end_angle,
            160,
            lv_palette_main(LV_PALETTE_GREEN)
        );


        // Red zone below minimum.
        create_zone_arc(
            state->oil_press_meter,
            0,
            90,
            0,
            config.engine_oil_pressure_min,
            oil_start_angle,
            oil_end_angle,
            160,
            lv_palette_main(LV_PALETTE_RED)
        );


        // Red zone above maximum.
        create_zone_arc(
            state->oil_press_meter,
            0,
            90,
            config.engine_oil_pressure_max,
            90,
            oil_start_angle,
            oil_end_angle,
            160,
            lv_palette_main(LV_PALETTE_RED)
        );


        state->oil_press_needle =
            create_needle(
                state->oil_press_meter,
                0,
                90,
                oil_start_angle,
                oil_end_angle,
                160
            );


        lv_obj_t *oil_press_label =
            lv_label_create(scr->screen);

        lv_obj_align(
            oil_press_label,
            LV_ALIGN_CENTER,
            -125,
            280
        );

#if LV_FONT_MONTSERRAT_32
        lv_obj_set_style_text_font(
            oil_press_label,
            &lv_font_montserrat_32,
            LV_PART_MAIN
        );
#endif

        lv_obj_set_style_text_color(
            oil_press_label,
            lv_color_black(),
            LV_PART_MAIN
        );

        lv_label_set_text_static(
            oil_press_label,
            "psi"
        );
    }


    // -------------------------------------------------------------------------
    // ENGINE TEMPERATURE
    // -------------------------------------------------------------------------

    state->eng_temp_meter =
        lv_obj_create(scr->screen);

    lv_obj_remove_style(
        state->eng_temp_meter,
        NULL,
        LV_PART_MAIN
    );

    lv_obj_set_size(
        state->eng_temp_meter,
        160,
        160
    );


    int temp_x;
    int temp_y = 190;

    if (config.engine_oil_pressure_enabled) {
        temp_x = 125;
    } else {
        temp_x = 0;
    }

    lv_obj_align(
        state->eng_temp_meter,
        LV_ALIGN_CENTER,
        temp_x,
        temp_y
    );


    const int temp_start_angle = 90;
    const int temp_end_angle   = 360;


    // Base temperature gauge.
    state->eng_temp_arc =
        create_gauge_arc(
            state->eng_temp_meter,
            0,
            120,
            temp_start_angle,
            temp_end_angle,
            160
        );


    // Green zone.
    create_zone_arc(
        state->eng_temp_meter,
        0,
        120,
        0,
        config.engine_temp_redline,
        temp_start_angle,
        temp_end_angle,
        160,
        lv_palette_main(LV_PALETTE_GREEN)
    );


    // Red zone.
    create_zone_arc(
        state->eng_temp_meter,
        0,
        120,
        config.engine_temp_redline,
        120,
        temp_start_angle,
        temp_end_angle,
        160,
        lv_palette_main(LV_PALETTE_RED)
    );


    state->eng_temp_needle =
        create_needle(
            state->eng_temp_meter,
            0,
            120,
            temp_start_angle,
            temp_end_angle,
            160
        );


    // Temperature units.
    lv_obj_t *eng_temp_label =
        lv_label_create(scr->screen);

    lv_obj_align(
        eng_temp_label,
        LV_ALIGN_CENTER,
        temp_x,
        290
    );

    lv_obj_set_style_text_font(
        eng_temp_label,
        &lv_font_montserrat_32,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        eng_temp_label,
        lv_color_black(),
        LV_PART_MAIN
    );

    lv_label_set_text_static(
        eng_temp_label,
        LV_SYMBOL_DEGREES "C"
    );


    // -------------------------------------------------------------------------
    // TOP LEFT INFORMATION
    // -------------------------------------------------------------------------

    state->eng_sog_label =
        lv_label_create(scr->screen);

    lv_obj_align(
        state->eng_sog_label,
        LV_ALIGN_TOP_LEFT,
        2,
        2
    );

    lv_obj_set_style_text_font(
        state->eng_sog_label,
        &lv_font_montserrat_30,
        LV_PART_MAIN
    );

    if (config.engine_top_left_enabled) {

        if (config.engine_top_left_metric ==
            EngineTopLeftMetric::SOG) {

            lv_label_set_text_static(
                state->eng_sog_label,
                "SOG (kt):\n--"
            );

        } else {

            lv_label_set_text_static(
                state->eng_sog_label,
                "THROTTLE (%):\n--"
            );
        }

    } else {

        lv_obj_add_flag(
            state->eng_sog_label,
            LV_OBJ_FLAG_HIDDEN
        );
    }


    // -------------------------------------------------------------------------
    // TOP RIGHT INFORMATION
    // -------------------------------------------------------------------------

    state->eng_alternator_label =
        lv_label_create(scr->screen);

    lv_obj_align(
        state->eng_alternator_label,
        LV_ALIGN_TOP_RIGHT,
        -2,
        2
    );

    lv_obj_set_style_text_font(
        state->eng_alternator_label,
        &lv_font_montserrat_30,
        LV_PART_MAIN
    );

    if (config.engine_top_right_enabled) {

        if (config.engine_top_right_metric ==
            EngineTopRightMetric::AlternatorVoltage) {

            lv_label_set_text_static(
                state->eng_alternator_label,
                "ALT (V):\n--"
            );

        } else {

            lv_label_set_text_static(
                state->eng_alternator_label,
                "BAT (V):\n--"
            );
        }

    } else {

        lv_obj_add_flag(
            state->eng_alternator_label,
            LV_OBJ_FLAG_HIDDEN
        );
    }


    // -------------------------------------------------------------------------
    // RPM NUMERIC VALUE
    // -------------------------------------------------------------------------

    state->rpm_value_label =
        lv_label_create(scr->screen);

    lv_obj_align(
        state->rpm_value_label,
        LV_ALIGN_BOTTOM_LEFT,
        8,
        -6
    );

    lv_obj_set_style_text_font(
        state->rpm_value_label,
        &lv_font_montserrat_26,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        state->rpm_value_label,
        lv_color_black(),
        LV_PART_MAIN
    );

    lv_label_set_text_static(
        state->rpm_value_label,
        "RPM: --"
    );


    // -------------------------------------------------------------------------
    // ENGINE ID
    // -------------------------------------------------------------------------

    state->engine_id_label =
        lv_label_create(scr->screen);

    lv_obj_align(
        state->engine_id_label,
        LV_ALIGN_BOTTOM_RIGHT,
        -5,
        -5
    );

    lv_obj_set_style_text_font(
        state->engine_id_label,
        &lv_font_montserrat_20,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        state->engine_id_label,
        lv_color_hex(0x999999),
        LV_PART_MAIN
    );

    String engine_id_str =
        String("E") +
        String(state->engine_id + 1);

    lv_label_set_text(
        state->engine_id_label,
        engine_id_str.c_str()
    );
}


// =============================================================================
// Update callback
// =============================================================================

static void engine_update_cb(lv_updatable_screen_t *scr)
{
    EngineScreenState *state =
        (EngineScreenState *)scr->user_data;

    if (!state || !scr->screen) {
        return;
    }


    // Update once per second.
    const uint32_t now = millis();

    if ((now - state->last_update_ms) < 1000) {
        return;
    }

    state->last_update_ms = now;


    const auto& config =
        get_signalk_path_config();


    if (!state->engine_rpm_needle ||
        !state->eng_temp_needle ||
        !state->eng_sog_label ||
        !state->eng_alternator_label ||
        !state->rpm_value_label) {

        return;
    }


    if (config.engine_oil_pressure_enabled &&
        !state->oil_press_needle) {

        return;
    }


    int engine_id = state->engine_id;

    if (engine_id < 0) {
        engine_id = 0;
    }

    if (engine_id >= 8) {
        engine_id = 7;
    }


    // -------------------------------------------------------------------------
    // RPM
    // -------------------------------------------------------------------------

    if (fresh(
        shipDataModel
            .propulsion
            .engines[engine_id]
            .revolutions_RPM
            .age)) {

        state->last_rpm =
            shipDataModel
                .propulsion
                .engines[engine_id]
                .revolutions_RPM
                .rpm;
    }


    float scaled_rpm =
        state->last_rpm;

    if (scaled_rpm < 0.0f) {
        scaled_rpm = 0.0f;
    }

    if (scaled_rpm > 6000.0f) {
        scaled_rpm = 6000.0f;
    }


    // Gauge is x100 RPM.
    float rpm_gauge_value =
        scaled_rpm / 100.0f;


    set_needle_value(
        state->engine_rpm_needle,
        rpm_gauge_value,
        0,
        60,
        150,
        390
    );


    lv_label_set_text(
        state->rpm_value_label,
        (String("RPM: ") +
         String(
             (int32_t)lroundf(
                 state->last_rpm
             )
         )).c_str()
    );


    // -------------------------------------------------------------------------
    // TOP LEFT
    // -------------------------------------------------------------------------

    if (config.engine_top_left_enabled) {

        if (config.engine_top_left_metric ==
            EngineTopLeftMetric::SOG) {

            float sog =
                shipDataModel
                    .navigation
                    .speed_over_ground
                    .kn;

            lv_label_set_text(
                state->eng_sog_label,
                (String("SOG (kt):\n    ") +
                 String(sog, 1)).c_str()
            );

        } else {

            if (fresh(
                shipDataModel
                    .propulsion
                    .engines[engine_id]
                    .throttle
                    .age)) {

                state->last_throttle =
                    shipDataModel
                        .propulsion
                        .engines[engine_id]
                        .throttle
                        .pct;
            }

            lv_label_set_text(
                state->eng_sog_label,
                (String("THROTTLE (%):\n    ") +
                 String(state->last_throttle, 0)).c_str()
            );
        }
    }


    // -------------------------------------------------------------------------
    // TOP RIGHT
    // -------------------------------------------------------------------------

    if (config.engine_top_right_enabled) {

        if (config.engine_top_right_metric ==
            EngineTopRightMetric::AlternatorVoltage) {

            if (fresh(
                shipDataModel
                    .propulsion
                    .engines[engine_id]
                    .alternator_voltage
                    .age)) {

                state->last_alternator =
                    shipDataModel
                        .propulsion
                        .engines[engine_id]
                        .alternator_voltage
                        .volt;
            }

            lv_label_set_text(
                state->eng_alternator_label,
                (String("ALT (V):\n    ") +
                 String(state->last_alternator, 1)).c_str()
            );

        } else {

            if (fresh(
                shipDataModel
                    .propulsion
                    .engines[engine_id]
                    .battery_voltage
                    .age)) {

                state->last_battery_voltage =
                    shipDataModel
                        .propulsion
                        .engines[engine_id]
                        .battery_voltage
                        .volt;
            }

            lv_label_set_text(
                state->eng_alternator_label,
                (String("BAT (V):\n    ") +
                 String(state->last_battery_voltage, 1)).c_str()
            );
        }
    }


    // -------------------------------------------------------------------------
    // OIL PRESSURE
    // -------------------------------------------------------------------------

    if (config.engine_oil_pressure_enabled) {

        if (fresh(
            shipDataModel
                .propulsion
                .engines[engine_id]
                .oil_pressure
                .age)) {

            state->last_oil_pressure =
                shipDataModel
                    .propulsion
                    .engines[engine_id]
                    .oil_pressure
                    .hPa *
                0.0145037738f;
        }


        if (state->last_oil_pressure < 0.0f) {
            state->last_oil_pressure = 0.0f;
        }

        if (state->last_oil_pressure > 90.0f) {
            state->last_oil_pressure = 90.0f;
        }


        set_needle_value(
            state->oil_press_needle,
            state->last_oil_pressure,
            0,
            90,
            90,
            360
        );
    }


    // -------------------------------------------------------------------------
    // ENGINE TEMPERATURE
    // -------------------------------------------------------------------------

    if (fresh(
        shipDataModel
            .propulsion
            .engines[engine_id]
            .temp_deg_C
            .age)) {

        state->last_temp =
            shipDataModel
                .propulsion
                .engines[engine_id]
                .temp_deg_C
                .deg_C;
    }


    if (state->last_temp < 0.0f) {
        state->last_temp = 0.0f;
    }

    if (state->last_temp > 120.0f) {
        state->last_temp = 120.0f;
    }


    set_needle_value(
        state->eng_temp_needle,
        state->last_temp,
        0,
        120,
        90,
        360
    );
}


// =============================================================================
// Factory function
// =============================================================================

void create_engine_screens(
    lv_updatable_screen_t **out_screens,
    int *out_count)
{
    const auto& config =
        get_signalk_path_config();

    int num_engines =
        config.num_engines;

    if (num_engines < 1) {
        num_engines = 1;
    }

    if (num_engines > MAX_ENGINE_SCREENS) {
        num_engines = MAX_ENGINE_SCREENS;
    }


    for (int i = 0; i < num_engines; i++) {

        // Determine engine ID from configuration.
        int engine_id = 0;

        if (i == 0) {

            engine_id =
                config.engine_screen_1_id;

        } else if (i == 1) {

            engine_id =
                config.engine_screen_2_id;

        } else {

            engine_id = i;
        }


        if (engine_id < 0) {
            engine_id = 0;
        }

        if (engine_id >= 8) {
            engine_id = 7;
        }


        // Initialize state.
        engine_states[i] = {};

        engine_states[i].engine_id =
            engine_id;


        // Initialize screen.
        engine_screens_array[i].screen =
            nullptr;

        engine_screens_array[i].created =
            false;

        engine_screens_array[i].create_cb =
            lv_engine_display;

        engine_screens_array[i].update_cb =
            engine_update_cb;

        engine_screens_array[i].user_data =
            &engine_states[i];


        out_screens[i] =
            &engine_screens_array[i];
    }


    *out_count =
        num_engines;

    engine_screens_created =
        num_engines;
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