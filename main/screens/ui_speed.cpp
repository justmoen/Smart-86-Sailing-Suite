#include <ui_screens.h>
#include <ship_data_model.h>
#include <ship_data_util.h>
#include <StreamString.h>
#include "ui_speed.h"
#include "ui_init.h"
#include "chart_data_history.h"
#include "signalk_path_config.h"

#ifdef __cplusplus
extern "C" {
#endif

static lv_obj_t *header_label;
static lv_obj_t *sog_label;
static lv_obj_t *sog_avg_label;
static lv_obj_t *spd_label;
static lv_obj_t *leeway_label;
static lv_obj_t *s_cogt_label;
static lv_obj_t *s_hdt_label;
static lv_obj_t *speed_chart = nullptr;

static lv_chart_series_t *speed_series = nullptr;
ChartDataHistory *speed_history = nullptr;

// Deferred chart update tracking
static float pending_speed_val = 0;
static bool pending_chart_add_point = false;
static int pending_chart_range_update = 0;

void speed_queue_chart_data(void)
{
    if (speed_history && fresh(shipDataModel.navigation.speed_over_ground.age)) {
        pending_speed_val = shipDataModel.navigation.speed_over_ground.kn;
        pending_chart_add_point = true;
        pending_chart_range_update++;
    }
}

/* -------------------------------------------------- */
/* UI Creation                                        */
/* -------------------------------------------------- */

static void lv_speed_display(lv_updatable_screen_t *scr)
{
    lv_obj_t *parent = scr->screen;

    header_label = lv_label_create(parent);
    lv_obj_align(header_label, LV_ALIGN_TOP_MID, 0, 20);
    lv_label_set_recolor(header_label, true);
    lv_obj_set_style_text_font(header_label, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_label_set_text_static(header_label, "#ffffff Speed #");


    sog_label = lv_label_create(parent);
    lv_obj_align(sog_label, LV_ALIGN_TOP_LEFT, 10, 80);
    lv_obj_set_style_text_font(sog_label, &lv_font_montserrat_32, 0);
    lv_label_set_text_static(sog_label, "SOG (kt):                       --");


    sog_avg_label = lv_label_create(parent);
    lv_obj_align(sog_avg_label, LV_ALIGN_TOP_LEFT, 10, 120);
    lv_obj_set_style_text_font(sog_avg_label, &lv_font_montserrat_32, 0);
    lv_label_set_text_static(sog_avg_label, "SOG AVG (kt):             --");

    spd_label = lv_label_create(parent);
    lv_obj_align(spd_label, LV_ALIGN_TOP_LEFT, 10, 160);
    lv_obj_set_style_text_font(spd_label, &lv_font_montserrat_32, 0);
    lv_label_set_text_static(spd_label, "SPD (kt):                       --");

    leeway_label = lv_label_create(parent);
    lv_obj_align(leeway_label, LV_ALIGN_TOP_LEFT, 10, 200);
    lv_obj_set_style_text_font(leeway_label, &lv_font_montserrat_32, 0);
    lv_label_set_text_static(leeway_label, "Leeway (est):              --");

    s_cogt_label = lv_label_create(parent);
    lv_obj_align(s_cogt_label, LV_ALIGN_TOP_LEFT, 10, 240);
    lv_obj_set_style_text_font(s_cogt_label, &lv_font_montserrat_32, 0);
    lv_label_set_text_static(s_cogt_label, "COGT:                            --");

    s_hdt_label = lv_label_create(parent);
    lv_obj_align(s_hdt_label, LV_ALIGN_TOP_LEFT, 10, 280);
    lv_obj_set_style_text_font(s_hdt_label, &lv_font_montserrat_32, 0);
    lv_label_set_text_static(s_hdt_label, "HDT:                               --");
    
    // Create speed chart in bottom half

    speed_chart = lv_chart_create(parent);
    lv_obj_set_size(speed_chart, 680, 200);
    lv_obj_align(speed_chart, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_chart_set_type(speed_chart, LV_CHART_TYPE_LINE);
    const auto& config = get_signalk_path_config();
    // High-res point count: 30 pts per minute, max 600, min 50
    int speed_point_count = std::min(600, std::max(50, config.speed_chart_duration * 30));
    lv_chart_set_point_count(speed_chart, speed_point_count);
    lv_chart_set_range(speed_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 20);
    lv_chart_set_div_line_count(speed_chart, 5, 5);
    
    // Y-axis label "Speed (kn)"
    lv_obj_t * y_label = lv_label_create(speed_chart);
    lv_label_set_text(y_label, "Speed kn");
    lv_obj_set_style_text_font(y_label, &lv_font_montserrat_24, 0);
    lv_obj_align(y_label, LV_ALIGN_LEFT_MID, 5, 0);
    
    // X-axis label "Time"
    lv_obj_t * x_label = lv_label_create(speed_chart);
    lv_label_set_text(x_label, "Time");
    lv_obj_set_style_text_font(x_label, &lv_font_montserrat_24, 0);
    lv_obj_align(x_label, LV_ALIGN_BOTTOM_MID, 0, 5);
    
    // Allow gesture events to pass through to screen
    lv_obj_clear_flag(speed_chart, LV_OBJ_FLAG_CLICKABLE);

    
    // Style the chart
    lv_obj_set_style_text_font(speed_chart, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_border_color(speed_chart, lv_color_hex(0xcccccc), LV_PART_MAIN);
    
    // Initialize history tracker with calculated point count
    if (speed_history == nullptr) {
        speed_history = new ChartDataHistory("speed", config.speed_chart_duration, speed_point_count);
    }
}

/* -------------------------------------------------- */
/* Screen Update                                      */
/* -------------------------------------------------- */

static void speed_update_cb(lv_updatable_screen_t *scr)
{
    lv_label_set_text(sog_label,
        (String("SOG (kt):                       ")
         += (fresh(shipDataModel.navigation.speed_over_ground.age)
              ? String(shipDataModel.navigation.speed_over_ground.kn, 1)
              : String("--"))).c_str());

    lv_label_set_text(sog_avg_label,
        (String("SOG AVG (kt):             ")
         += (fresh(shipDataModel.navigation.speed_over_ground_avg.age, 20000)
              ? String(shipDataModel.navigation.speed_over_ground_avg.kn, 1)
              : String("--"))).c_str());

    lv_label_set_text(spd_label,
        (String("SPD (kt):                       ")
         += (fresh(shipDataModel.navigation.speed_through_water.age)
              ? String(shipDataModel.navigation.speed_through_water.kn, 1)
              : String("--"))).c_str());

    lv_label_set_text(leeway_label,
        (String("Leeway (est):              ")
         += (fresh(shipDataModel.navigation.leeway.age)
              ? String(shipDataModel.navigation.leeway.deg, 1) + LV_SYMBOL_DEGREES
              : String("--"))).c_str());

    lv_label_set_text(s_cogt_label,
        (String("COGT:                            ")
         += (fresh(shipDataModel.navigation.course_over_ground_true.age)
              ? String(shipDataModel.navigation.course_over_ground_true.deg, 1) + LV_SYMBOL_DEGREES
              : String("--"))).c_str());

    lv_label_set_text(s_hdt_label,
        (String("HDT:                               ")
         += (fresh(shipDataModel.navigation.heading_true.age)
              ? String(shipDataModel.navigation.heading_true.deg, 1) + LV_SYMBOL_DEGREES
              : String("--"))).c_str());
    

}

/* -------------------------------------------------- */
/* Deferred Chart Updates (called outside display lock) */
/* -------------------------------------------------- */

void speed_process_deferred_chart_updates()
{
    if (!speed_history || !speed_chart) return;
    
    // Add pending point to chart
    static uint32_t last_chart_add = 0;
    if (pending_chart_add_point && (millis() - last_chart_add > 2000)) {
        pending_chart_add_point = false;
        last_chart_add = millis();
        
        // Create series once on first data point
        if (speed_series == nullptr) {
            speed_series = lv_chart_add_series(speed_chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
        }
        
        // Record in history
        speed_history->add_point(pending_speed_val);
        
        // Add point to chart if series exists
        if (speed_series != nullptr) {
            lv_chart_set_next_value(speed_chart, speed_series, (lv_coord_t)pending_speed_val);
        }
    }
    
    // Update range if needed (every ~1000ms based on counter)
    if (pending_chart_range_update >= 50) {  // 50 * 20ms = ~1000ms
        pending_chart_range_update = 0;
        
        ChartDataPoint points[600];
        int point_count = 0;
        speed_history->get_points(points, point_count, 600);
        
        if (point_count > 1) {
            // Find max for scaling
            float max_speed = 5;  // Minimum display range
            for (int i = 0; i < point_count; i++) {
                if (points[i].value > max_speed) max_speed = points[i].value;
            }
            max_speed *= 1.2f;  // Add 20% margin
            
            lv_chart_set_range(speed_chart, LV_CHART_AXIS_PRIMARY_Y, 0, (int)max_speed);
        }
    }
}

/* -------------------------------------------------- */
/* Screen Init                                        */
/* -------------------------------------------------- */

lv_updatable_screen_t speedScreen = {
    .screen = nullptr,
    .created = false,
    .create_cb = lv_speed_display,
    .update_cb = speed_update_cb
};

#ifdef __cplusplus
} /*extern "C"*/
#endif