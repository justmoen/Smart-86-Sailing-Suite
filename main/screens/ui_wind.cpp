#include "ui_wind.h"

#include <cstdio>
#include <cmath>

#include <ship_data_util.h>
#include <navigation.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// WIND DISPLAY GEOMETRY
// ============================================================================
#define WIND_DISPLAY_SIZE       680
#define WIND_CENTER             340

#define WIND_OUTER_RADIUS       300

#define WIND_TICK_OUTER         300
#define WIND_TICK_INNER         285
#define WIND_MAJOR_INNER        270

#define WIND_NEEDLE_LENGTH      260

#define WIND_TICK_COUNT         37
#define WIND_MAJOR_EVERY        3 // Every 30 degrees (3 * 10°)

// ============================================================================
// LVGL OBJECTS
// ============================================================================
static lv_obj_t *wind_display = nullptr;

static lv_obj_t *wind_needle_apparent = nullptr;
static lv_obj_t *wind_needle_true = nullptr;

static lv_obj_t *aws_label = nullptr;  // Top Left
static lv_obj_t *awa_label = nullptr;  // Top Right
static lv_obj_t *tws_label = nullptr;  // Bottom Left
static lv_obj_t *twd_label = nullptr;  // Bottom Right

// ============================================================================
// PERSISTENT LINE POINT ARRAYS
// ============================================================================
static lv_point_precise_t wind_tick_points[WIND_TICK_COUNT][2];
static lv_point_precise_t wind_needle_points[2][2];

// ============================================================================
// COORDINATE TRANSLATIONS
// ============================================================================
static float wind_angle_to_radians(float degrees)
{
    return (degrees - 90.0f) * (float)M_PI / 180.0f;
}

static lv_point_precise_t wind_point(float angle, float radius)
{
    const float radians = wind_angle_to_radians(angle);
    lv_point_precise_t p;
    p.x = WIND_CENTER + (lv_coord_t)(cosf(radians) * radius);
    p.y = WIND_CENTER + (lv_coord_t)(sinf(radians) * radius);
    return p;
}

static int wind_angle_to_lvgl_arc(float wind_angle)
{
    int angle = (int)lroundf(wind_angle + 270.0f);
    while (angle < 0) angle += 360;
    while (angle >= 360) angle -= 360;
    return angle;
}

static void set_wind_needle(lv_obj_t *needle, int needle_index, float angle, float length)
{
    if (!needle) return;

    wind_needle_points[needle_index][0].x = WIND_CENTER;
    wind_needle_points[needle_index][0].y = WIND_CENTER;
    wind_needle_points[needle_index][1] = wind_point(angle, length);

    lv_line_set_points_mutable(needle, wind_needle_points[needle_index], 2);
    lv_obj_set_pos(needle, 0, 0);
    lv_obj_invalidate(needle);
}

// ============================================================================
// ZONES, TICKS, AND LABELS CREATION
// ============================================================================
static lv_obj_t *create_wind_zone(lv_obj_t *parent, float wind_start, float wind_end, lv_color_t color)
{
    const int lvgl_start = wind_angle_to_lvgl_arc(wind_start);
    const int lvgl_end = wind_angle_to_lvgl_arc(wind_end);

    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_remove_style_all(arc);
    lv_obj_set_size(arc, WIND_OUTER_RADIUS * 2, WIND_OUTER_RADIUS * 2);
    lv_obj_center(arc);

    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_angles(arc, lvgl_start, lvgl_end);

    lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, false, LV_PART_INDICATOR);

    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_width(arc, 0, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    return arc;
}

static void create_wind_ticks(lv_obj_t *parent)
{
    for (int i = 0; i < WIND_TICK_COUNT; ++i) {
        const float angle = -180.0f + (i * 10.0f);
        const bool major = ((i % WIND_MAJOR_EVERY) == 0);
        const float inner = major ? WIND_MAJOR_INNER : WIND_TICK_INNER;

        wind_tick_points[i][0] = wind_point(angle, WIND_TICK_OUTER);
        wind_tick_points[i][1] = wind_point(angle, inner);

        lv_obj_t *line = lv_line_create(parent);
        lv_line_set_points(line, wind_tick_points[i], 2);
        lv_obj_set_pos(line, 0, 0);

        // Dynamic tick mark coloring matching zone parameters
        lv_color_t tick_color = lv_color_hex(0x606060); // Default dark grey
        if (angle >= -60.0f && angle <= -20.0f) {
            tick_color = lv_color_hex(0xD82C2C); // Match expanded Port Red
        } else if (angle >= 20.0f && angle <= 60.0f) {
            tick_color = lv_color_hex(0x2CD82C); // Match expanded Starboard Green
        }

        lv_obj_set_style_line_color(line, tick_color, LV_PART_MAIN);
        lv_obj_set_style_line_opa(line, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_line_width(line, major ? 4 : 2, LV_PART_MAIN);
        lv_obj_set_style_line_rounded(line, false, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(line, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    }
}

static void create_wind_scale_labels(lv_obj_t *parent)
{
    struct WindLabel {
        const char *text;
        float angle;
    };

    static const WindLabel labels[] = {
        { "0",    0.0f   },
        { "30",   30.0f  },
        { "60",   60.0f  },
        { "90",   90.0f  },
        { "120",  120.0f },
        { "150",  150.0f },
        { "180",  180.0f },
        { "-150", -150.0f},
        { "-120", -120.0f},
        { "-90",  -90.0f },
        { "-60",  -60.0f },
        { "-30",  -30.0f }
    };

    for (unsigned i = 0; i < sizeof(labels) / sizeof(labels[0]); ++i) {
        const lv_point_precise_t p = wind_point(labels[i].angle, 235);

        lv_obj_t *label = lv_label_create(parent);
        lv_obj_remove_style_all(label);
        lv_label_set_text(label, labels[i].text);

#if LV_FONT_MONTSERRAT_26
        lv_obj_set_style_text_font(label, &lv_font_montserrat_26, LV_PART_MAIN);
#endif
        lv_obj_set_style_text_color(label, lv_color_hex(0x202020), LV_PART_MAIN);
        lv_obj_set_style_text_opa(label, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);

        int offset_x = (labels[i].angle < 0 || labels[i].angle >= 100) ? -28 : -18;
        lv_obj_set_pos(label, p.x + offset_x, p.y - 15);
        lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    }
}

static lv_obj_t *create_wind_needle(lv_obj_t *parent, lv_color_t color, int width, int needle_index)
{
    wind_needle_points[needle_index][0].x = WIND_CENTER;
    wind_needle_points[needle_index][0].y = WIND_CENTER;
    wind_needle_points[needle_index][1] = wind_point(0.0f, WIND_NEEDLE_LENGTH);

    lv_obj_t *needle = lv_line_create(parent);
    lv_line_set_points_mutable(needle, wind_needle_points[needle_index], 2);
    lv_obj_set_pos(needle, 0, 0);

    lv_obj_set_style_line_color(needle, color, LV_PART_MAIN);
    lv_obj_set_style_line_opa(needle, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_line_width(needle, width, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(needle, true, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(needle, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(needle, LV_OBJ_FLAG_CLICKABLE);

    return needle;
}

static void create_wind_center(lv_obj_t *parent)
{
    lv_obj_t *hub = lv_obj_create(parent);
    lv_obj_remove_style_all(hub);
    lv_obj_set_size(hub, 24, 24);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(hub, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(hub, 0, LV_PART_MAIN);
    lv_obj_set_pos(hub, WIND_CENTER - 12, WIND_CENTER - 12);
    lv_obj_clear_flag(hub, LV_OBJ_FLAG_CLICKABLE);
}

// ============================================================================
// MAIN GENERATION INTERFACE
// ============================================================================
static void lv_wind_display(lv_updatable_screen_t *scr)
{
    lv_obj_set_style_bg_color(scr->screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr->screen, LV_OPA_COVER, LV_PART_MAIN);

    wind_display = lv_obj_create(scr->screen);
    lv_obj_remove_style_all(wind_display);
    lv_obj_set_size(wind_display, WIND_DISPLAY_SIZE, WIND_DISPLAY_SIZE);
    lv_obj_center(wind_display);

    lv_obj_set_style_bg_color(wind_display, lv_color_hex(0xF4F5F7), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(wind_display, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(wind_display, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(wind_display, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_clear_flag(wind_display, LV_OBJ_FLAG_CLICKABLE);

    // Expanded tack zones ending at -20 and 20 degrees
    create_wind_zone(wind_display, -60.0f, -20.0f, lv_color_hex(0xD82C2C)); // Port Red
    create_wind_zone(wind_display, 20.0f, 60.0f, lv_color_hex(0x2CD82C));   // Starboard Green

    create_wind_ticks(wind_display);
    create_wind_scale_labels(wind_display);

    // Needles (Index 0: Apparent Wind Dark Grey, Index 1: True Wind Orange pointer)
    wind_needle_apparent = create_wind_needle(wind_display, lv_color_hex(0x282828), 12, 0);
    wind_needle_true = create_wind_needle(wind_display, lv_color_hex(0xFF7A00), 14, 1);

    create_wind_center(wind_display);

    // ------------------------------------------------------------------------
    // Dashboard Data Text Labels
    // ------------------------------------------------------------------------
    // Top Left: Apparent Wind Speed (AWS)
    aws_label = lv_label_create(scr->screen);
    lv_obj_remove_style_all(aws_label);
    lv_obj_set_style_text_color(aws_label, lv_color_white(), LV_PART_MAIN);
#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(aws_label, &lv_font_montserrat_30, LV_PART_MAIN);
#endif
    lv_obj_align(aws_label, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_label_set_text_static(aws_label, "AWS: -- kt");

    // Top Right: Apparent Wind Angle/Direction (AWA)
    awa_label = lv_label_create(scr->screen);
    lv_obj_remove_style_all(awa_label);
    lv_obj_set_style_text_color(awa_label, lv_color_white(), LV_PART_MAIN);
#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(awa_label, &lv_font_montserrat_30, LV_PART_MAIN);
#endif
    lv_obj_align(awa_label, LV_ALIGN_TOP_RIGHT, -20, 20);
    lv_label_set_text_static(awa_label, "AWA: --°");

    // Bottom Left: True Wind Speed (TWS)
    tws_label = lv_label_create(scr->screen);
    lv_obj_remove_style_all(tws_label);
    lv_obj_set_style_text_color(tws_label, lv_color_white(), LV_PART_MAIN);
#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(tws_label, &lv_font_montserrat_30, LV_PART_MAIN);
#endif
    lv_obj_align(tws_label, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_label_set_text_static(tws_label, "TWS: -- kt");

    // Bottom Right: True Wind Direction (TWD)
    twd_label = lv_label_create(scr->screen);
    lv_obj_remove_style_all(twd_label);
    lv_obj_set_style_text_color(twd_label, lv_color_white(), LV_PART_MAIN);
#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(twd_label, &lv_font_montserrat_30, LV_PART_MAIN);
#endif
    lv_obj_align(twd_label, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_label_set_text_static(twd_label, "TWD: --°");
}

static void wind_update_cb(lv_updatable_screen_t *scr)
{
    (void)scr;
    if (!windScreen.screen || !aws_label || !awa_label || !tws_label || !twd_label || !wind_needle_apparent || !wind_needle_true) {
        return;
    }

    char buf[64];

    // 1. AWS Update (Top Left)
    if (fresh(shipDataModel.environment.wind.apparent_wind_speed.age)) {
        snprintf(buf, sizeof(buf), "AWS: %.1f kt", shipDataModel.environment.wind.apparent_wind_speed.kn);
    } else {
        snprintf(buf, sizeof(buf), "AWS: -- kt");
    }
    lv_label_set_text(aws_label, buf);

    // 2. AWA Update (Top Right)
    if (fresh(shipDataModel.environment.wind.apparent_wind_angle.age)) {
        snprintf(buf, sizeof(buf), "AWA: %.0f°", shipDataModel.environment.wind.apparent_wind_angle.deg);
    } else {
        snprintf(buf, sizeof(buf), "AWA: --°");
    }
    lv_label_set_text(awa_label, buf);

    // 3. TWS Update (Bottom Left)
    if (fresh(shipDataModel.environment.wind.true_wind_speed.age)) {
        snprintf(buf, sizeof(buf), "TWS: %.1f kt", shipDataModel.environment.wind.true_wind_speed.kn);
    } else {
        snprintf(buf, sizeof(buf), "TWS: -- kt");
    }
    lv_label_set_text(tws_label, buf);

    // 4. TWD Update (Bottom Right)
    if (fresh(shipDataModel.environment.wind.true_wind_angle.age)) {
        snprintf(buf, sizeof(buf), "TWA: %.0f°", shipDataModel.environment.wind.true_wind_angle.deg);
    } else {
        snprintf(buf, sizeof(buf), "TWA: --°");
    }
    lv_label_set_text(twd_label, buf);

    // Needle Vectors: Index 0 maps AWA, Index 1 tracks TWD (True Wind Direction/Angle)
    const float apparent_wind_angle = fresh(shipDataModel.environment.wind.apparent_wind_angle.age) ? shipDataModel.environment.wind.apparent_wind_angle.deg : 0.0f;
    set_wind_needle(wind_needle_apparent, 0, apparent_wind_angle, WIND_NEEDLE_LENGTH);

    const float true_wind_angle = fresh(shipDataModel.environment.wind.true_wind_angle.age) ? shipDataModel.environment.wind.true_wind_angle.deg : 0.0f;
    set_wind_needle(wind_needle_true, 1, true_wind_angle, WIND_NEEDLE_LENGTH);
}

// Screen registration binding structure
lv_updatable_screen_t windScreen = {
    .screen = nullptr,
    .created = false,
    .create_cb = lv_wind_display,
    .update_cb = wind_update_cb
};

#ifdef __cplusplus
}
#endif
