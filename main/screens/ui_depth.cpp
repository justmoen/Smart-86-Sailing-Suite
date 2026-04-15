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

static lv_obj_t *dbt_label;
static lv_obj_t *dbk_label;
static lv_obj_t *dbs_label;
static lv_obj_t *depth_gradient_label;
static lv_obj_t *d_heel_label;
static lv_obj_t *depth_chart = nullptr;
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

    lv_obj_t *main_label = lv_label_create(parent);
    lv_obj_align(main_label, LV_ALIGN_CENTER, 0, -105);
    lv_label_set_recolor(main_label, true);
#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(main_label, &lv_font_montserrat_30, LV_PART_MAIN);
#endif
    lv_label_set_text_static(main_label, "DEPTH  #0080ff " LV_SYMBOL_DOWNLOAD " #");

    dbt_label = lv_label_create(parent);
    lv_obj_align(dbt_label, LV_ALIGN_TOP_LEFT, 10, 40);
#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(dbt_label, &lv_font_montserrat_30, LV_PART_MAIN);
#endif
    lv_label_set_text_static(dbt_label, "DBT (ft):          --");

    dbk_label = lv_label_create(parent);
    lv_obj_align(dbk_label, LV_ALIGN_TOP_LEFT, 10, 80);
#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(dbk_label, &lv_font_montserrat_30, LV_PART_MAIN);
#endif
    lv_label_set_text_static(dbk_label, "DBK (ft):          --");

    dbs_label = lv_label_create(parent);
    lv_obj_align(dbs_label, LV_ALIGN_TOP_LEFT, 10, 120);
#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(dbs_label, &lv_font_montserrat_30, LV_PART_MAIN);
#endif
    lv_label_set_text_static(dbs_label, "DBS (ft):          --");

    depth_gradient_label = lv_label_create(parent);
    lv_obj_align(depth_gradient_label, LV_ALIGN_TOP_LEFT, 10, 160);
#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(depth_gradient_label, &lv_font_montserrat_30, LV_PART_MAIN);
#endif
    lv_label_set_text_static(depth_gradient_label, "Gradient:        --");

    d_heel_label = lv_label_create(parent);
    lv_obj_align(d_heel_label, LV_ALIGN_TOP_LEFT, 10, 200);
#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(d_heel_label, &lv_font_montserrat_30, LV_PART_MAIN);
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
    lv_obj_set_style_text_font(y_label, &lv_font_montserrat_12, 0);
    lv_obj_align(y_label, LV_ALIGN_LEFT_MID, 5, 0);
    
    // X-axis label "Time"
    lv_obj_t * x_label = lv_label_create(depth_chart);
    lv_label_set_text(x_label, "Time");
    lv_obj_set_style_text_font(x_label, &lv_font_montserrat_12, 0);
    lv_obj_align(x_label, LV_ALIGN_BOTTOM_MID, 0, 5);
    
    // Allow gesture events to pass through to screen
    lv_obj_clear_flag(depth_chart, LV_OBJ_FLAG_CLICKABLE);
    
    // Style the chart
    lv_obj_set_style_text_font(depth_chart, &lv_font_montserrat_14, LV_PART_MAIN);
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
    auto format_depth = [&](const char* label_prefix, float depth_m, uint32_t age) -> String {
        if (!fresh(age)) {
            return String(label_prefix) + "--";
        }
        
        String label(label_prefix);
        if ((int)config.distance_unit == 0) {  // Meters
            label += String(depth_m, 1) + "m";
        } else {  // Feet
            label += String(depth_m * _GPS_FEET_PER_METER, 1) + "ft";
        }
        return label;
    };

    lv_label_set_text(dbt_label, format_depth("DBT: ", shipDataModel.environment.depth.below_transducer.m, shipDataModel.environment.depth.below_transducer.age).c_str());

    lv_label_set_text(dbk_label, format_depth("DBK: ", shipDataModel.environment.depth.below_keel.m, shipDataModel.environment.depth.below_keel.age).c_str());

    lv_label_set_text(dbs_label, format_depth("DBS: ", shipDataModel.environment.depth.below_surface.m, shipDataModel.environment.depth.below_surface.age).c_str());

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
    if (depth_history && fresh(shipDataModel.environment.depth.below_transducer.age)) {
        float depth_val = shipDataModel.environment.depth.below_transducer.m;
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