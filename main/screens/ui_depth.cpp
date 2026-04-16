#include <ui_screens.h>
#include <ship_data_model.h>
#include <ship_data_util.h>
#include <StreamString.h>
#include "ui_depth.h"
#include "ui_init.h"
#include <TinyGPSPlus.h>
#include "chart_data_history.h"
#include "signalk_path_config.h"

#ifdef __cplusplus
extern "C" {
#endif

static lv_obj_t *header_label;
static lv_obj_t *depth_value_label;
static lv_obj_t *depth_gradient_label;
static lv_obj_t *d_heel_label;
static lv_obj_t *depth_chart = nullptr;

struct BestDepth {
    float m;
    bool valid;
};

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
static ChartDataHistory *depth_history = nullptr;

// Deferred chart update tracking
static float pending_depth_val = 0;

static bool pending_chart_add_point = false;
static int pending_chart_range_update = 0;

/* -------------------------------------------------- */
/* UI Creation                                        */
/* -------------------------------------------------- */

static void lv_depth_display(lv_updatable_screen_t *scr)
{
    lv_obj_t *parent = scr->screen;

    header_label = lv_label_create(parent);
    lv_obj_align(header_label, LV_ALIGN_TOP_MID, 0, 20);

    lv_label_set_recolor(header_label, true);
#if LV_FONT_MONTSERRAT_32
    lv_obj_set_style_text_font(header_label, &lv_font_montserrat_32, LV_PART_MAIN);
#endif
    lv_label_set_text_static(header_label, "#ffffff Depth #");  


    depth_value_label = lv_label_create(parent);
    lv_obj_align(depth_value_label, LV_ALIGN_TOP_LEFT, 10, 60);
#if LV_FONT_MONTSERRAT_32
    lv_obj_set_style_text_font(depth_value_label, &lv_font_montserrat_32, LV_PART_MAIN);
#endif
    lv_label_set_text_static(depth_value_label, "Depth:          --");  
    depth_gradient_label = lv_label_create(parent);
    lv_obj_align(depth_gradient_label, LV_ALIGN_TOP_LEFT, 10, 120);

#if LV_FONT_MONTSERRAT_32
    lv_obj_set_style_text_font(depth_gradient_label, &lv_font_montserrat_32, LV_PART_MAIN);
#endif
    lv_label_set_text_static(depth_gradient_label, "Gradient:        --");

    d_heel_label = lv_label_create(parent);
    lv_obj_align(d_heel_label, LV_ALIGN_TOP_LEFT, 10, 160);

#if LV_FONT_MONTSERRAT_32
    lv_obj_set_style_text_font(d_heel_label, &lv_font_montserrat_32, LV_PART_MAIN);
#endif
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
    lv_chart_add_series(depth_chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    
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
    lv_obj_clear_flag(depth_chart, LV_OBJ_FLAG_CLICKABLE);
    
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
    
    // Queue chart update for deferred processing (outside display lock)
    if (depth_history && bd.valid) {
        float depth_val = bd.m;
        if ((int)config.distance_unit == 1) {  // Feet
            depth_val *= _GPS_FEET_PER_METER;
        }
        pending_depth_val = depth_val;
        pending_chart_add_point = true;
        pending_chart_range_update++;
    }

}

/* -------------------------------------------------- */
/* Deferred Chart Updates (called outside display lock) */
/* -------------------------------------------------- */

void depth_process_deferred_chart_updates()
{
    if (!depth_history || !depth_chart) return;
    
    // Add pending point to chart
    static uint32_t last_chart_add = 0;
    if (pending_chart_add_point && (millis() - last_chart_add > 2000)) {
        pending_chart_add_point = false;
        last_chart_add = millis();
        
        // Create series once on first data point
        if (depth_series == nullptr) {
            depth_series = lv_chart_add_series(depth_chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
        }
        
        // Record in history
        depth_history->add_point(pending_depth_val);  // Positive for history
        
        // Add point to chart if series exists - invert for visual (shallow high Y)
        static float cur_chart_bottom = 25.0f;
        if (depth_series != nullptr) {
            lv_chart_set_next_value(depth_chart, depth_series, (lv_coord_t)(cur_chart_bottom - pending_depth_val));
        }
    }
    
    // Update range if needed (every ~1000ms based on counter)
    if (pending_chart_range_update >= 50) {  // 50 * 20ms = ~1000ms
        pending_chart_range_update = 0;
        
        ChartDataPoint points[600];
        int point_count = 0;
        depth_history->get_points(points, point_count, 600);
        
        if (point_count > 1) {
            // Depth chart: max depth bottom, 0 surface top, 10% margin above max depth
            // Negative depths: find min (greatest magnitude depth)
            float max_depth = 0.0f;
            for (int i = 0; i < point_count; i++) {
                max_depth = std::max(max_depth, points[i].value);
            }
            
            static float chart_bottom = 20.0f;
            if (max_depth > chart_bottom) {
                chart_bottom = max_depth * 1.05f;  // +5% margin
            } else {
                chart_bottom *= 0.95f;  // Shrink 5%
            }
            if (chart_bottom < 20.0f) chart_bottom = 20.0f;
            
            lv_chart_set_range(depth_chart, LV_CHART_AXIS_PRIMARY_Y, 0, (int)chart_bottom);
            Serial.printf("Depth: max=%.1f bottom=%.1f range=0-%d\n", max_depth, chart_bottom, (int)chart_bottom);
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