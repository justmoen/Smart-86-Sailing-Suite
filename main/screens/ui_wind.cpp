#include "ui_wind.h"
#include <cstdio>
#include <cmath>
#include <ship_data_util.h>
#include <navigation.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * LVGL 9 Wind Display
 *
 * Replaces the LVGL 8 lv_meter implementation with LVGL 9
 * primitives.
 *
 * Instrument:
 *   - 680 x 680
 *   - 0 degrees at top
 *   - -180 .. +180 degree scale
 *   - Red zone:   -60 .. -20
 *   - Green zone:  20 .. 60
 *   - Grey needle: apparent wind
 *   - Orange needle: ground wind
 * ================================================================ */


/* -------------------------------------------------- */
/* Configuration                                     */
/* -------------------------------------------------- */

#define WIND_DISPLAY_SIZE       680
#define WIND_CENTER             340

#define WIND_OUTER_RADIUS       300
#define WIND_TICK_OUTER         300
#define WIND_TICK_INNER         282
#define WIND_MAJOR_INNER        270

#define WIND_NEEDLE_LENGTH      260

#define WIND_TICK_COUNT         37
#define WIND_MAJOR_EVERY        3


/* -------------------------------------------------- */
/* LVGL objects                                       */
/* -------------------------------------------------- */

static lv_obj_t *wind_display;

static lv_obj_t *wind_scale;
static lv_obj_t *wind_needle_apparent;
static lv_obj_t *wind_needle_ground;

static lv_obj_t *wind_label;
static lv_obj_t *spd_w_label;
static lv_obj_t *gws_label;
static lv_obj_t *gwdt_label;


/* -------------------------------------------------- */
/* Helpers                                            */
/* -------------------------------------------------- */

/*
 * Convert a wind angle into a screen angle.
 *
 * 0 degrees = straight up.
 * Positive angles rotate clockwise.
 */
static float wind_angle_to_radians(float degrees)
{
    return (degrees - 90.0f) * (float)M_PI / 180.0f;
}


/*
 * Convert wind angle into an LVGL coordinate.
 */
static lv_point_precise_t wind_point(float angle, float radius)
{
    float radians = wind_angle_to_radians(angle);

    lv_point_precise_t p;

    p.x = WIND_CENTER +
          (lv_coord_t)(cosf(radians) * radius);

    p.y = WIND_CENTER +
          (lv_coord_t)(sinf(radians) * radius);

    return p;
}


/*
 * Set the end point of a wind needle.
 */
static void set_wind_needle(
    lv_obj_t *needle,
    float angle,
    float length)
{
    lv_point_precise_t points[2];

    points[0].x = WIND_CENTER;
    points[0].y = WIND_CENTER;

    points[1] = wind_point(angle, length);

    lv_line_set_points(
        needle,
        points,
        2);
}


/* -------------------------------------------------- */
/* Arc drawing helper                                 */
/* -------------------------------------------------- */

/*
 * LVGL 9 arc objects are used for the colored wind
 * sectors.
 *
 * The arc range is expressed in screen degrees.
 */
static lv_obj_t *create_wind_zone(
    lv_obj_t *parent,
    int start_angle,
    int end_angle,
    lv_color_t color)
{
    lv_obj_t *arc =
        lv_arc_create(parent);

    lv_obj_remove_style_all(arc);

    lv_obj_set_size(
        arc,
        WIND_OUTER_RADIUS * 2,
        WIND_OUTER_RADIUS * 2);

    lv_obj_center(arc);

    lv_arc_set_bg_angles(
        arc,
        0,
        360);

    lv_arc_set_angles(
        arc,
        start_angle,
        end_angle);

    lv_obj_set_style_arc_width(
        arc,
        8,
        LV_PART_INDICATOR);

    lv_obj_set_style_arc_color(
        arc,
        color,
        LV_PART_INDICATOR);

    lv_obj_set_style_arc_rounded(
        arc,
        false,
        LV_PART_INDICATOR);

    lv_obj_clear_flag(
        arc,
        LV_OBJ_FLAG_CLICKABLE);

    return arc;
}


/* -------------------------------------------------- */
/* Tick marks                                         */
/* -------------------------------------------------- */

static void create_wind_ticks(lv_obj_t *parent)
{
    /*
     * 37 ticks over -180 .. +180.
     *
     * This gives 10 degree spacing.
     */
    for (int i = 0; i < WIND_TICK_COUNT; ++i) {

        float angle =
            -180.0f + (i * 10.0f);

        bool major =
            ((i % WIND_MAJOR_EVERY) == 0);

        float inner =
            major
                ? WIND_MAJOR_INNER
                : WIND_TICK_INNER;

        lv_point_precise_t points[2];

        points[0] =
            wind_point(angle, WIND_TICK_OUTER);

        points[1] =
            wind_point(angle, inner);

        lv_obj_t *line =
            lv_line_create(parent);

        lv_line_set_points(
            line,
            points,
            2);

        lv_obj_set_style_line_color(
            line,
            lv_palette_main(LV_PALETTE_GREY),
            LV_PART_MAIN);

        lv_obj_set_style_line_width(
            line,
            major ? 4 : 2,
            LV_PART_MAIN);

        lv_obj_set_style_line_rounded(
            line,
            true,
            LV_PART_MAIN);

        lv_obj_clear_flag(
            line,
            LV_OBJ_FLAG_CLICKABLE);
    }
}


/* -------------------------------------------------- */
/* Scale labels                                       */
/* -------------------------------------------------- */

static void create_wind_scale_labels(lv_obj_t *parent)
{
    /*
     * Cardinal wind directions.
     *
     * These are intentionally kept simple because the
     * original LVGL meter did not display numeric values.
     */

    static const struct {
        const char *text;
        float angle;
    } labels[] = {
        { "0",    0.0f   },
        { "90",   90.0f  },
        { "180",  180.0f },
        { "-90", -90.0f }
    };

    for (unsigned i = 0;
         i < sizeof(labels) / sizeof(labels[0]);
         ++i) {

        lv_point_precise_t p =
            wind_point(
                labels[i].angle,
                245);

        lv_obj_t *label =
            lv_label_create(parent);

        lv_label_set_text(
            label,
            labels[i].text);

        lv_obj_set_style_text_font(
            label,
            &lv_font_montserrat_26,
            LV_PART_MAIN);

        lv_obj_set_style_text_color(
            label,
            lv_palette_main(LV_PALETTE_GREY),
            LV_PART_MAIN);

        lv_obj_set_pos(
            label,
            p.x - 20,
            p.y - 15);

        lv_obj_clear_flag(
            label,
            LV_OBJ_FLAG_CLICKABLE);
    }
}


/* -------------------------------------------------- */
/* Needles                                            */
/* -------------------------------------------------- */

static lv_obj_t *create_wind_needle(
    lv_obj_t *parent,
    lv_color_t color,
    int width)
{
    lv_obj_t *needle =
        lv_line_create(parent);

    lv_point_precise_t points[2] = {
        {
            WIND_CENTER,
            WIND_CENTER
        },
        {
            WIND_CENTER,
            WIND_CENTER - WIND_NEEDLE_LENGTH
        }
    };

    lv_line_set_points(
        needle,
        points,
        2);

    lv_obj_set_style_line_color(
        needle,
        color,
        LV_PART_MAIN);

    lv_obj_set_style_line_width(
        needle,
        width,
        LV_PART_MAIN);

    lv_obj_set_style_line_rounded(
        needle,
        true,
        LV_PART_MAIN);

    lv_obj_clear_flag(
        needle,
        LV_OBJ_FLAG_CLICKABLE);

    return needle;
}


/* -------------------------------------------------- */
/* Center hub                                         */
/* -------------------------------------------------- */

static void create_wind_center(lv_obj_t *parent)
{
    lv_obj_t *hub =
        lv_obj_create(parent);

    lv_obj_remove_style_all(hub);

    lv_obj_set_size(
        hub,
        28,
        28);

    lv_obj_set_style_radius(
        hub,
        LV_RADIUS_CIRCLE,
        LV_PART_MAIN);

    lv_obj_set_style_bg_color(
        hub,
        lv_palette_main(LV_PALETTE_GREY),
        LV_PART_MAIN);

    lv_obj_set_pos(
        hub,
        WIND_CENTER - 14,
        WIND_CENTER - 14);

    lv_obj_clear_flag(
        hub,
        LV_OBJ_FLAG_CLICKABLE);
}


/* -------------------------------------------------- */
/* UI Creation                                        */
/* -------------------------------------------------- */

static void lv_wind_display(lv_updatable_screen_t *scr)
{
    wind_display =
        lv_obj_create(scr->screen);

    lv_obj_remove_style_all(
        wind_display);

    lv_obj_set_size(
        wind_display,
        WIND_DISPLAY_SIZE,
        WIND_DISPLAY_SIZE);

    lv_obj_center(
        wind_display);

    /*
     * Allow screen swipe/gesture events to pass through.
     */
    lv_obj_remove_flag(
        wind_display,
        LV_OBJ_FLAG_CLICKABLE);


    /* ------------------------------------------------
     * Colored wind zones
     * ------------------------------------------------ */

    /*
     * LVGL arc angles are screen angles.
     *
     * Our wind coordinate system has 0 at the top,
     * so convert the wind angles by adding 90 degrees.
     */

    create_wind_zone(
        wind_display,
        30,     // -60 degrees
        70,     // -20 degrees
        lv_palette_main(LV_PALETTE_RED));


    create_wind_zone(
        wind_display,
        110,    // +20 degrees
        150,    // +60 degrees
        lv_palette_main(LV_PALETTE_GREEN));


    /* ------------------------------------------------
     * Tick marks
     * ------------------------------------------------ */

    create_wind_ticks(
        wind_display);

    create_wind_scale_labels(
        wind_display);


    /* ------------------------------------------------
     * Needles
     * ------------------------------------------------ */

    wind_needle_apparent =
        create_wind_needle(
            wind_display,
            lv_palette_main(LV_PALETTE_GREY),
            10);

    wind_needle_ground =
        create_wind_needle(
            wind_display,
            lv_palette_main(LV_PALETTE_ORANGE),
            10);


    create_wind_center(
        wind_display);


    /* ------------------------------------------------
     * Labels
     * ------------------------------------------------ */

    wind_label =
        lv_label_create(scr->screen);

    lv_obj_align(
        wind_label,
        LV_ALIGN_TOP_LEFT,
        5,
        2);

#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(
        wind_label,
        &lv_font_montserrat_30,
        0);
#endif

    lv_label_set_text_static(
        wind_label,
        "AWS: --\nkt");


    spd_w_label =
        lv_label_create(scr->screen);

    lv_obj_align(
        spd_w_label,
        LV_ALIGN_TOP_RIGHT,
        -5,
        2);

#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(
        spd_w_label,
        &lv_font_montserrat_30,
        0);
#endif

    lv_label_set_text_static(
        spd_w_label,
        "SPD: --\nkt");


    gws_label =
        lv_label_create(scr->screen);

    lv_obj_align(
        gws_label,
        LV_ALIGN_BOTTOM_LEFT,
        5,
        -2);

#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(
        gws_label,
        &lv_font_montserrat_30,
        0);
#endif

    lv_label_set_text_static(
        gws_label,
        "GWS:\n-- kt");


    gwdt_label =
        lv_label_create(scr->screen);

    lv_obj_align(
        gwdt_label,
        LV_ALIGN_BOTTOM_RIGHT,
        -5,
        -2);

#if LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(
        gwdt_label,
        &lv_font_montserrat_30,
        0);
#endif

    lv_label_set_text_static(
        gwdt_label,
        "GWD:\n--" LV_SYMBOL_DEGREES "t");
}


/* -------------------------------------------------- */
/* Screen Update                                      */
/* -------------------------------------------------- */

static void wind_update_cb(lv_updatable_screen_t *scr)
{
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


    /* ------------------------------------------------
     * Apparent wind speed
     * ------------------------------------------------ */

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
                .kn);
    }
    else {
        snprintf(
            buf,
            sizeof(buf),
            "AWS: --\nkt");
    }

    lv_label_set_text(
        wind_label,
        buf);


    /* ------------------------------------------------
     * Speed through water
     * ------------------------------------------------ */

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
                .kn);
    }
    else {
        snprintf(
            buf,
            sizeof(buf),
            "SPD: --\nkt");
    }

    lv_label_set_text(
        spd_w_label,
        buf);


    /* ------------------------------------------------
     * Ground wind speed
     * ------------------------------------------------ */

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
                .kn);
    }
    else {
        snprintf(
            buf,
            sizeof(buf),
            "GWS:\n-- kt");
    }

    lv_label_set_text(
        gws_label,
        buf);


    /* ------------------------------------------------
     * Ground wind direction
     * ------------------------------------------------ */

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
                .deg);
    }
    else {
        snprintf(
            buf,
            sizeof(buf),
            "GWD:\n--" LV_SYMBOL_DEGREES "t");
    }

    lv_label_set_text(
        gwdt_label,
        buf);


    /* ------------------------------------------------
     * Apparent wind needle
     * ------------------------------------------------ */

    float apparent_wind_angle =
        fresh(
            shipDataModel
                .environment
                .wind
                .apparent_wind_angle
                .age)
            ? shipDataModel
                  .environment
                  .wind
                  .apparent_wind_angle
                  .deg
            : 0.0f;

    set_wind_needle(
        wind_needle_apparent,
        apparent_wind_angle,
        WIND_NEEDLE_LENGTH);


    /* ------------------------------------------------
     * Ground wind needle
     * ------------------------------------------------ */

    float ground_wind_angle =
        fresh(
            shipDataModel
                .environment
                .wind
                .ground_wind_angle
                .age)
            ? shipDataModel
                  .environment
                  .wind
                  .ground_wind_angle
                  .deg
            : 0.0f;

    set_wind_needle(
        wind_needle_ground,
        ground_wind_angle,
        WIND_NEEDLE_LENGTH);
}


/* -------------------------------------------------- */
/* Screen Init                                        */
/* -------------------------------------------------- */

lv_updatable_screen_t windScreen = {
    .screen = nullptr,
    .created = false,
    .create_cb = lv_wind_display,
    .update_cb = wind_update_cb
};


#ifdef __cplusplus
}
#endif