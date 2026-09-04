#include <ui_screens.h>
#include <ship_data_model.h>
#include <ship_data_util.h>
#include <StreamString.h>
#include "ui_depth.h"
#include "ui_init.h"
#include <TinyGPSPlus.h>
#include "chart_data_history.h"
#include "signalk_path_config.h"
#include "ui_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

static lv_obj_t *header_label;
static lv_obj_t *depth_value_label;
static lv_obj_t *depth_gradient_label;
static lv_obj_t *d_heel_label;
static lv_obj_t *depth_chart = nullptr;

BestDepth get_best_depth() {
    const ship_data_t& data = shipDataModel;
    if (fresh(data.environment.depth.below_keel.age)) {
        return {data.environment.depth.below_keel.m, true};
    } else if (fresh(data.environment.depth.below_transducer.age)) {
        return {data.environment.depth.below_transducer.m, true};
    } else if (fresh(data.environment.depth.below_surface.age)) {
        return {data.environment.depth.below_surface.m, true};
    }
    return {0.0f, false};
}

static lv_chart_series_t *depth_series = nullptr;
ChartDataHistory *depth_history = nullptr;

// Deferred chart update tracking
static float pending_depth_val = 0;
static bool pending_chart_add_point = false;
static bool pending_chart_range_update = false;
static bool depth_chart_loaded = false;
static float depth_chart_bottom = 20.0f;

void depth_queue_chart_data(void)
{
    if (!depth_history) {
        return;
    }

    BestDepth bd =
        get_best_depth();

    if (!bd.valid) {
        return;
    }

    const auto& config =
        get_signalk_path_config();

    float depth_val =
        bd.m;

    if ((int)config.distance_unit == 1) {
        depth_val *=
            _GPS_FEET_PER_METER;
    }

    /*
     * History collection is independent of screen visibility.
     */
    if (depth_history->add_point(depth_val)) {

        pending_depth_val =
            depth_val;

        pending_chart_add_point =
            true;

        pending_chart_range_update =
            true;
    }
}

/* -------------------------------------------------- */
/* UI Creation                                        */
/* -------------------------------------------------- */

static void lv_depth_display(lv_updatable_screen_t *scr)
{
    lv_obj_t *parent = scr->screen;

    header_label = lv_label_create(parent);
    lv_obj_align(header_label, LV_ALIGN_TOP_MID, 0, 20);

    lv_label_set_recolor(header_label, true);
    lv_obj_set_style_text_font(header_label, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_label_set_text_static(header_label, "#ffffff Depth #");  


    depth_value_label = lv_label_create(parent);
    lv_obj_align(depth_value_label, LV_ALIGN_TOP_LEFT, 10, 60);
    lv_obj_set_style_text_font(depth_value_label, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_label_set_text_static(depth_value_label, "Depth:          --");  
    depth_gradient_label = lv_label_create(parent);
    lv_obj_align(depth_gradient_label, LV_ALIGN_TOP_LEFT, 10, 120);

    lv_obj_set_style_text_font(depth_gradient_label, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_label_set_text_static(depth_gradient_label, "Gradient:        --");

    d_heel_label = lv_label_create(parent);
    lv_obj_align(d_heel_label, LV_ALIGN_TOP_LEFT, 10, 160);

    lv_obj_set_style_text_font(d_heel_label, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_label_set_text_static(d_heel_label, "Heel:                 --");
    
    // Create depth chart in bottom half
    const auto& config = get_signalk_path_config();
    depth_chart = lv_chart_create(parent);
    lv_obj_set_size(depth_chart, 680, 200);
    lv_obj_align(depth_chart, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_chart_set_type(depth_chart, LV_CHART_TYPE_LINE);
    // High-res point count: 30 pts per minute, max 600, min 50
    int depth_point_count = std::min(600, std::max(50, config.depth_chart_duration * 30));
    lv_chart_set_point_count(depth_chart, depth_point_count);
    lv_chart_set_range(depth_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 25);
    lv_chart_set_div_line_count(depth_chart, 5, 8);  // More Y ticks
    lv_chart_set_update_mode(
        depth_chart,
        LV_CHART_UPDATE_MODE_SHIFT
    );

    depth_series =
        lv_chart_add_series(
            depth_chart,
            lv_palette_main(LV_PALETTE_BLUE),
            LV_CHART_AXIS_PRIMARY_Y
        );

    depth_chart_loaded = false;
    depth_chart_bottom = 20.0f;
    
    // Y-axis label "Depth (m)"
    lv_obj_t * y_label = lv_label_create(depth_chart);
    lv_label_set_text(y_label, "Depth m");
    lv_obj_set_style_text_font(y_label, &lv_font_montserrat_24, 0);
    lv_obj_align(y_label, LV_ALIGN_LEFT_MID, 5, 0);
    
    // X-axis label "Time"
    lv_obj_t * x_label = lv_label_create(depth_chart);
    lv_label_set_text(x_label, "Time");
    lv_obj_set_style_text_font(x_label, &lv_font_montserrat_24, 0);
    lv_obj_align(x_label, LV_ALIGN_BOTTOM_MID, 0, 5);
    
    // Allow gesture events to pass through to screen
    lv_obj_remove_flag(depth_chart, LV_OBJ_FLAG_CLICKABLE);
    
    // Style the chart
    lv_obj_set_style_text_font(depth_chart, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_border_color(depth_chart, lv_color_hex(0xcccccc), LV_PART_MAIN);
    
    // Initialize history tracker with calculated point count
    if (depth_history == nullptr) {
        depth_history = new ChartDataHistory("depth", config.depth_chart_duration, depth_point_count);
    }
}

/* -------------------------------------------------- */
/* Screen Update                                      */
/* -------------------------------------------------- */

static void depth_update_cb(lv_updatable_screen_t *scr)
{
    const auto& config = get_signalk_path_config();
    
    // Helper lambda to format depth values based on unit setting


    BestDepth bd = get_best_depth();
    String unit_str;
    if ((int)config.distance_unit == 0) {  // Meters
        unit_str = "m";
    } else {  // Feet
        unit_str = "ft";
    }
    String depth_text;
    if (bd.valid) {
        float display_val = bd.m;
        if (unit_str == "ft") display_val *= _GPS_FEET_PER_METER;
        depth_text = "Depth: " + String(display_val, 1) + unit_str;
    } else {
        depth_text = "Depth: --";
    }
    lv_label_set_text(depth_value_label, depth_text.c_str());

    lv_label_set_text(d_heel_label,
        (String("Heel:                 ")
         += (fresh(shipDataModel.navigation.attitude.heel.age)
              ? String(shipDataModel.navigation.attitude.heel.deg, 1) + LV_SYMBOL_DEGREES
              : String("--"))).c_str());

    lv_label_set_text(depth_gradient_label,
        (String("Gradient:        ")
         += (fresh(shipDataModel.environment.depth_gradient.age)
              ? String(shipDataModel.environment.depth_gradient.deg, 1) + LV_SYMBOL_DEGREES
              : String("--"))).c_str());
    


}

/* -------------------------------------------------- */
/* Deferred Chart Updates (called outside display lock) */
/* -------------------------------------------------- */

void depth_process_deferred_chart_updates()
{
    if (!depth_history || !depth_chart) {
        return;
    }

    /*
     * History continues regardless of screen visibility.
     *
     * Do not touch LVGL unless Depth is the active screen.
     */
    if (!ui_manager_is_current_screen(&depthScreen)) {
        return;
    }

    if (!depth_series) {
        return;
    }


    // -------------------------------------------------------------------------
    // First time Depth becomes visible:
    // populate it from the continuously collected history.
    // -------------------------------------------------------------------------

    if (!depth_chart_loaded) {

        ChartDataPoint points[600];
        int point_count = 0;

        depth_history->get_points(
            points,
            point_count,
            600
        );

        // Determine range from history.
        float max_depth = 0.0f;

        for (
            int i = 0;
            i < point_count;
            i++
        ) {
            max_depth =
                std::max(
                    max_depth,
                    points[i].value
                );
        }

        depth_chart_bottom =
            std::max(
                20.0f,
                max_depth * 1.05f
            );

        lv_chart_set_range(
            depth_chart,
            LV_CHART_AXIS_PRIMARY_Y,
            0,
            (int)depth_chart_bottom
        );


        uint32_t chart_count =
            lv_chart_get_point_count(
                depth_chart
            );

        int32_t *y_points =
            lv_chart_get_y_array(
                depth_chart,
                depth_series
            );

        if (y_points) {

            for (
                uint32_t i = 0;
                i < chart_count;
                i++
            ) {
                y_points[i] =
                    LV_CHART_POINT_NONE;
            }

            uint32_t count =
                std::min(
                    (uint32_t)point_count,
                    chart_count
                );

            for (
                uint32_t i = 0;
                i < count;
                i++
            ) {
                float display_value =
                    depth_chart_bottom -
                    points[i].value;

                y_points[i] =
                    (int32_t)display_value;
            }

            lv_chart_set_x_start_point(
                depth_chart,
                depth_series,
                0
            );

            lv_chart_refresh(
                depth_chart
            );
        }

        depth_chart_loaded =
            true;

        pending_chart_add_point =
            false;

        pending_chart_range_update =
            false;

        return;
    }


    // -------------------------------------------------------------------------
    // Live chart update.
    // -------------------------------------------------------------------------

    if (pending_chart_add_point) {

        pending_chart_add_point =
            false;

        float chart_value =
            depth_chart_bottom -
            pending_depth_val;

        lv_chart_set_next_value(
            depth_chart,
            depth_series,
            (lv_coord_t)chart_value
        );
    }


    // -------------------------------------------------------------------------
    // Range update only when a new history sample was accepted.
    // -------------------------------------------------------------------------

    if (pending_chart_range_update) {

        pending_chart_range_update =
            false;

        ChartDataPoint points[600];
        int point_count = 0;

        depth_history->get_points(
            points,
            point_count,
            600
        );

        if (point_count > 1) {

            float max_depth = 0.0f;

            for (
                int i = 0;
                i < point_count;
                i++
            ) {
                max_depth =
                    std::max(
                        max_depth,
                        points[i].value
                    );
            }

            depth_chart_bottom =
                std::max(
                    20.0f,
                    max_depth * 1.05f
                );

            lv_chart_set_range(
                depth_chart,
                LV_CHART_AXIS_PRIMARY_Y,
                0,
                (int)depth_chart_bottom
            );
        }
    }
}

/* -------------------------------------------------- */
/* Screen Init                                        */
/* -------------------------------------------------- */

lv_updatable_screen_t depthScreen = {
    .screen = nullptr,
    .created = false,
    .create_cb = lv_depth_display,
    .update_cb = depth_update_cb
};

#ifdef __cplusplus
} /*extern "C"*/
#endif