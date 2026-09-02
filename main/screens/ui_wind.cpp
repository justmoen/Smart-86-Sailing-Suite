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
//
// Our wind-angle convention:
//
//                         0°
//                          ↑
//                          |
//                -50°     |     +50°
//                   ↖     |     ↗
//                          |
//             -90° ←──────┼──────→ +90°
//                          |
//                          ↓
//                        180°
//
// Positive wind angles rotate clockwise.
//
// LVGL arc-angle convention:
//
//                         270°
//                           ↑
//                           |
//                  220°    |    320°
//                     ↖    |    ↗
//                           |
//              180° ←──────┼──────→ 0°
//                           |
//                           ↓
//                          90°
//
// Therefore:
//
//     wind  -50° = LVGL arc 220°
//     wind  -15° = LVGL arc 255°
//     wind    0° = LVGL arc 270°
//     wind  +15° = LVGL arc 285°
//     wind  +50° = LVGL arc 320°
//
// ============================================================================

#define WIND_DISPLAY_SIZE       680
#define WIND_CENTER             340

#define WIND_OUTER_RADIUS       300

#define WIND_TICK_OUTER         300
#define WIND_TICK_INNER         282
#define WIND_MAJOR_INNER        270

#define WIND_NEEDLE_LENGTH      260

#define WIND_TICK_COUNT         37
#define WIND_MAJOR_EVERY        3


// ============================================================================
// LVGL OBJECTS
// ============================================================================

static lv_obj_t *wind_display = nullptr;

static lv_obj_t *wind_needle_apparent = nullptr;
static lv_obj_t *wind_needle_ground = nullptr;

static lv_obj_t *wind_label = nullptr;
static lv_obj_t *spd_w_label = nullptr;
static lv_obj_t *gws_label = nullptr;
static lv_obj_t *gwdt_label = nullptr;


// ============================================================================
// PERSISTENT LINE POINT ARRAYS
// ============================================================================
//
// LVGL 9 stores the POINTER passed to lv_line_set_points() rather than
// copying the array. These must therefore remain alive for the lifetime
// of the line objects.
//
// ============================================================================

static lv_point_precise_t wind_tick_points[WIND_TICK_COUNT][2];

static lv_point_precise_t wind_needle_points[2][2];


// ============================================================================
// WIND ANGLE -> CARTESIAN SCREEN POSITION
// ============================================================================
//
// Our coordinate system:
//
//     0°    = up
//     +90°  = right
//     +180° = down
//     -90°  = left
//
// Screen coordinates:
//
//     +X = right
//     +Y = down
//
// ============================================================================

static float wind_angle_to_radians(float degrees)
{
    return (degrees - 90.0f) * (float)M_PI / 180.0f;
}


static lv_point_precise_t wind_point(float angle, float radius)
{
    const float radians =
        wind_angle_to_radians(angle);

    lv_point_precise_t p;

    p.x =
        WIND_CENTER +
        (lv_coord_t)(cosf(radians) * radius);

    p.y =
        WIND_CENTER +
        (lv_coord_t)(sinf(radians) * radius);

    return p;
}


// ============================================================================
// WIND ANGLE -> LVGL ARC ANGLE
// ============================================================================
//
// LVGL arc:
//
//     0°   = 3 o'clock
//     90°  = 6 o'clock
//     180° = 9 o'clock
//     270° = 12 o'clock
//
// Our wind:
//
//     0° = 12 o'clock
//
// Therefore:
//
//     LVGL = wind + 270
//
// ============================================================================

static int wind_angle_to_lvgl_arc(float wind_angle)
{
    int angle =
        (int)lroundf(wind_angle + 270.0f);

    while (angle < 0)
        angle += 360;

    while (angle >= 360)
        angle -= 360;

    return angle;
}


// ============================================================================
// UPDATE A NEEDLE
// ============================================================================
//
// IMPORTANT:
// The line object itself is positioned at 0,0.
//
// The points are already expressed in wind_display coordinates.
//
// Therefore:
//
//     center = (340,340)
//
// and the tip is calculated directly in that same coordinate system.
//
// ============================================================================

static void set_wind_needle(
    lv_obj_t *needle,
    int needle_index,
    float angle,
    float length)
{
    if (!needle)
        return;

    wind_needle_points[needle_index][0].x =
        WIND_CENTER;

    wind_needle_points[needle_index][0].y =
        WIND_CENTER;

    wind_needle_points[needle_index][1] =
        wind_point(angle, length);

    lv_line_set_points_mutable(
        needle,
        wind_needle_points[needle_index],
        2
    );

    // The line uses coordinates relative to its own origin.
    // Keep that origin at the upper-left of wind_display.
    lv_obj_set_pos(
        needle,
        0,
        0
    );

    lv_obj_invalidate(needle);
}


// ============================================================================
// CREATE A WIND ZONE
// ============================================================================
//
// wind_start/end are OUR wind angles.
//
// They are converted to LVGL's arc-angle system.
//
// ============================================================================

static lv_obj_t *create_wind_zone(
    lv_obj_t *parent,
    float wind_start,
    float wind_end,
    lv_color_t color)
{
    const int lvgl_start =
        wind_angle_to_lvgl_arc(wind_start);

    const int lvgl_end =
        wind_angle_to_lvgl_arc(wind_end);

    lv_obj_t *arc =
        lv_arc_create(parent);

    lv_obj_remove_style_all(arc);

    lv_obj_set_size(
        arc,
        WIND_OUTER_RADIUS * 2,
        WIND_OUTER_RADIUS * 2
    );

    lv_obj_center(arc);

    // No visible background arc.
    lv_arc_set_bg_angles(
        arc,
        0,
        360
    );

    // LVGL angles increase clockwise.
    lv_arc_set_angles(
        arc,
        lvgl_start,
        lvgl_end
    );

    lv_obj_set_style_arc_width(
        arc,
        8,
        LV_PART_INDICATOR
    );

    lv_obj_set_style_arc_color(
        arc,
        color,
        LV_PART_INDICATOR
    );

    lv_obj_set_style_arc_opa(
        arc,
        LV_OPA_COVER,
        LV_PART_INDICATOR
    );

    lv_obj_set_style_arc_rounded(
        arc,
        false,
        LV_PART_INDICATOR
    );

    // Hide the background arc.
    lv_obj_set_style_arc_opa(
        arc,
        LV_OPA_TRANSP,
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        arc,
        LV_OPA_TRANSP,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        arc,
        0,
        LV_PART_MAIN
    );

    // Hide the knob.
    lv_obj_set_style_bg_opa(
        arc,
        LV_OPA_TRANSP,
        LV_PART_KNOB
    );

    lv_obj_set_style_border_width(
        arc,
        0,
        LV_PART_KNOB
    );

    lv_obj_clear_flag(
        arc,
        LV_OBJ_FLAG_CLICKABLE
    );

    return arc;
}


// ============================================================================
// CREATE TICK MARKS
// ============================================================================
//
// The tick points are calculated directly in wind_display coordinates.
//
// The line objects themselves remain at:
//
//     x = 0
//     y = 0
//
// Do NOT translate them to their minimum point.
//
// ============================================================================

static void create_wind_ticks(lv_obj_t *parent)
{
    for (int i = 0; i < WIND_TICK_COUNT; ++i) {

        const float angle =
            -180.0f + (i * 10.0f);

        const bool major =
            ((i % WIND_MAJOR_EVERY) == 0);

        const float inner =
            major
                ? WIND_MAJOR_INNER
                : WIND_TICK_INNER;


        // Persistent point array.
        wind_tick_points[i][0] =
            wind_point(
                angle,
                WIND_TICK_OUTER
            );

        wind_tick_points[i][1] =
            wind_point(
                angle,
                inner
            );


        lv_obj_t *line =
            lv_line_create(parent);


        // LVGL 9 automatically sizes the line based on its points.
        lv_line_set_points(
            line,
            wind_tick_points[i],
            2
        );


        // IMPORTANT:
        // The points are already in wind_display coordinates.
        //
        // The line therefore stays at the origin of wind_display.
        lv_obj_set_pos(
            line,
            0,
            0
        );


        lv_obj_set_style_line_color(
            line,
            lv_color_black(),
            LV_PART_MAIN
        );

        lv_obj_set_style_line_opa(
            line,
            LV_OPA_COVER,
            LV_PART_MAIN
        );

        lv_obj_set_style_line_width(
            line,
            major ? 4 : 2,
            LV_PART_MAIN
        );

        lv_obj_set_style_line_rounded(
            line,
            false,
            LV_PART_MAIN
        );

        lv_obj_set_style_bg_opa(
            line,
            LV_OPA_TRANSP,
            LV_PART_MAIN
        );

        lv_obj_clear_flag(
            line,
            LV_OBJ_FLAG_CLICKABLE
        );
    }
}


// ============================================================================
// CREATE SCALE LABELS
// ============================================================================

static void create_wind_scale_labels(lv_obj_t *parent)
{
    struct WindLabel {
        const char *text;
        float angle;
    };

    static const WindLabel labels[] = {
        { "0",   0.0f   },
        { "90",  90.0f  },
        { "180", 180.0f },
        { "-90", -90.0f }
    };


    for (unsigned i = 0;
         i < sizeof(labels) / sizeof(labels[0]);
         ++i) {

        const lv_point_precise_t p =
            wind_point(
                labels[i].angle,
                245
            );


        lv_obj_t *label =
            lv_label_create(parent);

        lv_obj_remove_style_all(label);

        lv_label_set_text(
            label,
            labels[i].text
        );


#if LV_FONT_MONTSERRAT_26

        lv_obj_set_style_text_font(
            label,
            &lv_font_montserrat_26,
            LV_PART_MAIN
        );

#endif


        lv_obj_set_style_text_color(
            label,
            lv_color_black(),
            LV_PART_MAIN
        );

        lv_obj_set_style_text_opa(
            label,
            LV_OPA_COVER,
            LV_PART_MAIN
        );

        lv_obj_set_style_bg_opa(
            label,
            LV_OPA_TRANSP,
            LV_PART_MAIN
        );


        // Approximate center alignment around the calculated point.
        lv_obj_set_pos(
            label,
            p.x - 20,
            p.y - 15
        );

        lv_obj_clear_flag(
            label,
            LV_OBJ_FLAG_CLICKABLE
        );
    }
}


// ============================================================================
// CREATE NEEDLE
// ============================================================================
//
// The needle starts at the exact center:
//
//     (340,340)
//
// Initial angle is 0°:
//
//     (340,80)
//
// ============================================================================

static lv_obj_t *create_wind_needle(
    lv_obj_t *parent,
    lv_color_t color,
    int width,
    int needle_index)
{
    wind_needle_points[needle_index][0].x =
        WIND_CENTER;

    wind_needle_points[needle_index][0].y =
        WIND_CENTER;


    wind_needle_points[needle_index][1] =
        wind_point(
            0.0f,
            WIND_NEEDLE_LENGTH
        );


    lv_obj_t *needle =
        lv_line_create(parent);


    lv_line_set_points_mutable(
        needle,
        wind_needle_points[needle_index],
        2
    );


    // The line's coordinate system starts at the gauge's origin.
    lv_obj_set_pos(
        needle,
        0,
        0
    );


    lv_obj_set_style_line_color(
        needle,
        color,
        LV_PART_MAIN
    );

    lv_obj_set_style_line_opa(
        needle,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_line_width(
        needle,
        width,
        LV_PART_MAIN
    );

    lv_obj_set_style_line_rounded(
        needle,
        true,
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        needle,
        LV_OPA_TRANSP,
        LV_PART_MAIN
    );

    lv_obj_clear_flag(
        needle,
        LV_OBJ_FLAG_CLICKABLE
    );


    return needle;
}


// ============================================================================
// CREATE CENTER HUB
// ============================================================================

static void create_wind_center(lv_obj_t *parent)
{
    lv_obj_t *hub =
        lv_obj_create(parent);

    lv_obj_remove_style_all(hub);

    lv_obj_set_size(
        hub,
        28,
        28
    );

    lv_obj_set_style_radius(
        hub,
        LV_RADIUS_CIRCLE,
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_color(
        hub,
        lv_color_black(),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        hub,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        hub,
        0,
        LV_PART_MAIN
    );

    lv_obj_set_pos(
        hub,
        WIND_CENTER - 14,
        WIND_CENTER - 14
    );

    lv_obj_clear_flag(
        hub,
        LV_OBJ_FLAG_CLICKABLE
    );
}


// ============================================================================
// CREATE WIND SCREEN
// ============================================================================

static void lv_wind_display(lv_updatable_screen_t *scr)
{
    // ------------------------------------------------------------------------
    // Screen background
    // ------------------------------------------------------------------------

    lv_obj_set_style_bg_color(
        scr->screen,
        lv_color_black(),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        scr->screen,
        LV_OPA_COVER,
        LV_PART_MAIN
    );


    // ------------------------------------------------------------------------
    // Gauge face
    // ------------------------------------------------------------------------

    wind_display =
        lv_obj_create(scr->screen);

    lv_obj_remove_style_all(
        wind_display
    );

    lv_obj_set_size(
        wind_display,
        WIND_DISPLAY_SIZE,
        WIND_DISPLAY_SIZE
    );

    lv_obj_center(
        wind_display
    );

    lv_obj_set_style_bg_color(
        wind_display,
        lv_color_hex(0xD0D0D0),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        wind_display,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        wind_display,
        0,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        wind_display,
        LV_RADIUS_CIRCLE,
        LV_PART_MAIN
    );

    lv_obj_clear_flag(
        wind_display,
        LV_OBJ_FLAG_CLICKABLE
    );


    // ------------------------------------------------------------------------
    // Wind zones
    //
    // RED:
    //     wind -50° -> -15°
    //     LVGL 220° -> 255°
    //
    // GREEN:
    //     wind +15° -> +50°
    //     LVGL 285° -> 320°
    // ------------------------------------------------------------------------

    create_wind_zone(
        wind_display,
        -50.0f,
        -15.0f,
        lv_color_hex(0xE00000)
    );

    create_wind_zone(
        wind_display,
        15.0f,
        50.0f,
        lv_color_hex(0x00A000)
    );


    // ------------------------------------------------------------------------
    // Tick marks
    // ------------------------------------------------------------------------

    create_wind_ticks(
        wind_display
    );


    // ------------------------------------------------------------------------
    // Scale labels
    // ------------------------------------------------------------------------

    create_wind_scale_labels(
        wind_display
    );


    // ------------------------------------------------------------------------
    // Needles
    //
    // Apparent wind: dark grey
    // Ground wind:   orange
    // ------------------------------------------------------------------------

    wind_needle_apparent =
        create_wind_needle(
            wind_display,
            lv_color_hex(0x303030),
            14,
            0
        );

    wind_needle_ground =
        create_wind_needle(
            wind_display,
            lv_color_hex(0xFF6600),
            10,
            1
        );


    // ------------------------------------------------------------------------
    // Center hub
    // ------------------------------------------------------------------------

    create_wind_center(
        wind_display
    );


    // ------------------------------------------------------------------------
    // AWS
    // ------------------------------------------------------------------------

    wind_label =
        lv_label_create(scr->screen);

    lv_obj_remove_style_all(
        wind_label
    );

    lv_obj_set_style_text_color(
        wind_label,
        lv_color_white(),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_opa(
        wind_label,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

#if LV_FONT_MONTSERRAT_30

    lv_obj_set_style_text_font(
        wind_label,
        &lv_font_montserrat_30,
        LV_PART_MAIN
    );

#endif

    lv_obj_align(
        wind_label,
        LV_ALIGN_TOP_LEFT,
        5,
        2
    );

    lv_label_set_text_static(
        wind_label,
        "AWS: --\nkt"
    );


    // ------------------------------------------------------------------------
    // Speed through water
    // ------------------------------------------------------------------------

    spd_w_label =
        lv_label_create(scr->screen);

    lv_obj_remove_style_all(
        spd_w_label
    );

    lv_obj_set_style_text_color(
        spd_w_label,
        lv_color_white(),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_opa(
        spd_w_label,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

#if LV_FONT_MONTSERRAT_30

    lv_obj_set_style_text_font(
        spd_w_label,
        &lv_font_montserrat_30,
        LV_PART_MAIN
    );

#endif

    lv_obj_align(
        spd_w_label,
        LV_ALIGN_TOP_RIGHT,
        -5,
        2
    );

    lv_label_set_text_static(
        spd_w_label,
        "SPD: --\nkt"
    );


    // ------------------------------------------------------------------------
    // Ground wind speed
    // ------------------------------------------------------------------------

    gws_label =
        lv_label_create(scr->screen);

    lv_obj_remove_style_all(
        gws_label
    );

    lv_obj_set_style_text_color(
        gws_label,
        lv_color_white(),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_opa(
        gws_label,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

#if LV_FONT_MONTSERRAT_30

    lv_obj_set_style_text_font(
        gws_label,
        &lv_font_montserrat_30,
        LV_PART_MAIN
    );

#endif

    lv_obj_align(
        gws_label,
        LV_ALIGN_BOTTOM_LEFT,
        5,
        -2
    );

    lv_label_set_text_static(
        gws_label,
        "GWS:\n-- kt"
    );


    // ------------------------------------------------------------------------
    // Ground wind direction
    // ------------------------------------------------------------------------

    gwdt_label =
        lv_label_create(scr->screen);

    lv_obj_remove_style_all(
        gwdt_label
    );

    lv_obj_set_style_text_color(
        gwdt_label,
        lv_color_white(),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_opa(
        gwdt_label,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

#if LV_FONT_MONTSERRAT_30

    lv_obj_set_style_text_font(
        gwdt_label,
        &lv_font_montserrat_30,
        LV_PART_MAIN
    );

#endif

    lv_obj_align(
        gwdt_label,
        LV_ALIGN_BOTTOM_RIGHT,
        -5,
        -2
    );

    lv_label_set_text_static(
        gwdt_label,
        "GWD:\n--" LV_SYMBOL_DEGREES "t"
    );
}


// ============================================================================
// UPDATE WIND DATA
// ============================================================================

static void wind_update_cb(lv_updatable_screen_t *scr)
{
    (void)scr;

    if (!windScreen.screen)
        return;

    if (!wind_label ||
        !spd_w_label ||
        !gws_label ||
        !gwdt_label ||
        !wind_needle_apparent ||
        !wind_needle_ground) {

        return;
    }


    char buf[64];


    // ------------------------------------------------------------------------
    // Apparent wind speed
    // ------------------------------------------------------------------------

    if (fresh(
            shipDataModel
                .environment
                .wind
                .apparent_wind_speed
                .age)) {

        snprintf(
            buf,
            sizeof(buf),
            "AWS: %.1f\nkt",
            shipDataModel
                .environment
                .wind
                .apparent_wind_speed
                .kn
        );

    }
    else {

        snprintf(
            buf,
            sizeof(buf),
            "AWS: --\nkt"
        );
    }

    lv_label_set_text(
        wind_label,
        buf
    );


    // ------------------------------------------------------------------------
    // Speed through water
    // ------------------------------------------------------------------------

    if (fresh(
            shipDataModel
                .navigation
                .speed_through_water
                .age)) {

        snprintf(
            buf,
            sizeof(buf),
            "SPD: %.1f\nkt",
            shipDataModel
                .navigation
                .speed_through_water
                .kn
        );

    }
    else {

        snprintf(
            buf,
            sizeof(buf),
            "SPD: --\nkt"
        );
    }

    lv_label_set_text(
        spd_w_label,
        buf
    );


    // ------------------------------------------------------------------------
    // Ground wind speed
    // ------------------------------------------------------------------------

    if (fresh(
            shipDataModel
                .environment
                .wind
                .ground_wind_speed
                .age)) {

        snprintf(
            buf,
            sizeof(buf),
            "GWS:\n%.1f kt",
            shipDataModel
                .environment
                .wind
                .ground_wind_speed
                .kn
        );

    }
    else {

        snprintf(
            buf,
            sizeof(buf),
            "GWS:\n-- kt"
        );
    }

    lv_label_set_text(
        gws_label,
        buf
    );


    // ------------------------------------------------------------------------
    // Ground wind direction
    // ------------------------------------------------------------------------

    if (fresh(
            shipDataModel
                .environment
                .wind
                .ground_wind_dir_true
                .age)) {

        snprintf(
            buf,
            sizeof(buf),
            "GWD:\n%.0f" LV_SYMBOL_DEGREES "t",
            shipDataModel
                .environment
                .wind
                .ground_wind_dir_true
                .deg
        );

    }
    else {

        snprintf(
            buf,
            sizeof(buf),
            "GWD:\n--" LV_SYMBOL_DEGREES "t"
        );
    }

    lv_label_set_text(
        gwdt_label,
        buf
    );


    // ------------------------------------------------------------------------
    // Apparent wind angle
    //
    // 0°   = straight up
    // +90° = right
    // -90° = left
    // ------------------------------------------------------------------------

    const float apparent_wind_angle =
        fresh(
            shipDataModel
                .environment
                .wind
                .apparent_wind_angle
                .age
        )
        ? shipDataModel
              .environment
              .wind
              .apparent_wind_angle
              .deg
        : 0.0f;


    set_wind_needle(
        wind_needle_apparent,
        0,
        apparent_wind_angle,
        WIND_NEEDLE_LENGTH
    );


    // ------------------------------------------------------------------------
    // Ground wind angle
    // ------------------------------------------------------------------------

    const float ground_wind_angle =
        fresh(
            shipDataModel
                .environment
                .wind
                .ground_wind_angle
                .age
        )
        ? shipDataModel
              .environment
              .wind
              .ground_wind_angle
              .deg
        : 0.0f;


    set_wind_needle(
        wind_needle_ground,
        1,
        ground_wind_angle,
        WIND_NEEDLE_LENGTH
    );
}


// ============================================================================
// SCREEN REGISTRATION
// ============================================================================

lv_updatable_screen_t windScreen = {
    .screen = nullptr,
    .created = false,
    .create_cb = lv_wind_display,
    .update_cb = wind_update_cb
};


#ifdef __cplusplus
}
#endif