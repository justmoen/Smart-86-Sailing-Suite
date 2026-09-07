#include <ui_screens.h>
#include <ship_data_model.h>
#include <ship_data_util.h>
#include <StreamString.h>
#include "ui_heel.h"
#include "ui_init.h"

#include <cmath>
#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif


/* ================================================================
 * Heel / Clinometer display
 *
 * LVGL 9 replacement for the LVGL 8 lv_meter implementation.
 *
 * Scale:
 *   -45 .. +45 degrees
 *   19 ticks
 *   5 degree spacing
 *   major tick every 3 ticks
 *
 * Orientation:
 *   0 degrees  = straight DOWN
 *   -45 degrees = lower LEFT
 *   +45 degrees = lower RIGHT
 *
 * Needle:
 *   Orange
 *
 * Background:
 *   Grey
 * ================================================================ */


/* -------------------------------------------------- */
/* Configuration                                     */
/* -------------------------------------------------- */

#define HEEL_DISPLAY_SIZE       700

#define HEEL_CENTER_X           340
#define HEEL_CENTER_Y           340

#define HEEL_TICK_OUTER         320
#define HEEL_TICK_INNER         300
#define HEEL_MAJOR_INNER        280

#define HEEL_SCALE_LABEL_OUTER      345

#define HEEL_NEEDLE_LENGTH      290

#define HEEL_TICK_COUNT         19
#define HEEL_MAJOR_EVERY        3


/* -------------------------------------------------- */
/* LVGL objects                                       */
/* -------------------------------------------------- */

static lv_obj_t *heel_display = nullptr;

static lv_obj_t *heel_scale = nullptr;
static lv_obj_t *heel_needle = nullptr;

static lv_obj_t *pitch_label = nullptr;
static lv_obj_t *heel_leeway_label = nullptr;
static lv_obj_t *heel_drift_label = nullptr;
static lv_obj_t *heel_set_label = nullptr;
static lv_obj_t *heel_main_label = nullptr;


/* -------------------------------------------------- */
/* Helpers                                            */
/* -------------------------------------------------- */

/*
 * Convert heel angle to screen coordinates.
 *
 * We want:
 *
 *      -45       0       +45
 *        \       |       /
 *         \      |      /
 *          \     |     /
 *           \    |    /
 *            \   |   /
 *             \  |  /
 *              \ | /
 *               \|/
 *
 *              DOWN
 *
 * Screen coordinates:
 *
 *      0 degrees = right
 *     90 degrees = down
 *
 * Therefore:
 *
 *     heel 0°  -> screen 90°
 *     heel -45 -> screen 135°
 *     heel +45 -> screen 45°
 *
 * This is:
 *
 *     screen_angle = 90 - heel
 */
static float heel_angle_to_radians(float angle)
{
    return (90.0f - angle) *
           (float)M_PI /
           180.0f;
}


/*
 * Calculate a point on the heel scale.
 *
 * Points are expressed in heel_scale / heel_display
 * coordinates.
 */
static lv_point_precise_t heel_point(
    float angle,
    float radius)
{
    const float radians =
        heel_angle_to_radians(angle);

    lv_point_precise_t p;

    p.x =
        (lv_coord_t)(HEEL_CENTER_X +
        cosf(radians) * radius);

    p.y =
        (lv_coord_t)(HEEL_CENTER_Y +
        sinf(radians) * radius);

    return p;
}


/* -------------------------------------------------- */
/* Needle                                             */
/* -------------------------------------------------- */

static void set_heel_value(float value)
{
    if (!heel_needle) {
        return;
    }

    /*
     * Clamp the value to the displayed scale.
     */
    if (value > 45.0f) {
        value = 45.0f;
    }

    if (value < -45.0f) {
        value = -45.0f;
    }

    static lv_point_precise_t points[2];

    /*
     * Needle starts at the center.
     */
    points[0].x = HEEL_CENTER_X;
    points[0].y = HEEL_CENTER_Y;

    /*
     * Needle points downward at 0° heel.
     */
    points[1] =
        heel_point(
            value,
            HEEL_NEEDLE_LENGTH);

    /*
     * LVGL 9:
     * The needle is updated repeatedly, so use mutable
     * point storage.
     */
    lv_line_set_points_mutable(
        heel_needle,
        points,
        2);

    /*
     * Points are already in heel_display coordinates.
     */
    lv_obj_set_pos(
        heel_needle,
        0,
        0);
}


/* -------------------------------------------------- */
/* Scale                                              */
/* -------------------------------------------------- */

static void create_heel_scale(lv_obj_t *parent)
{
    /* 
     * We need 19 lines. Each line requires 2 points, plus 
     * an LV_COORD_MIN marker between segments to break the drawing path.
     * Formula: 19 segments * 3 slots = 57 entries (minus 1 for the final tail trailing slot)
     */
    constexpr size_t total_points = (HEEL_TICK_COUNT * 3) - 1;
    static lv_point_precise_t scale_points[total_points];

    size_t pt_idx = 0;

    for (int i = 0; i < HEEL_TICK_COUNT; ++i) {
        const float angle = -45.0f + ((float)i * 5.0f);
        const bool major = ((i % HEEL_MAJOR_EVERY) == 0);
        const float inner = major ? HEEL_MAJOR_INNER : HEEL_TICK_INNER;

        // Start point of the segment
        scale_points[pt_idx] = heel_point(angle, HEEL_TICK_OUTER);
        pt_idx++;

        // End point of the segment
        scale_points[pt_idx] = heel_point(angle, inner);
        pt_idx++;

        // Insert break token between segments, skipping the final element boundary
        if (i < HEEL_TICK_COUNT - 1) {
            scale_points[pt_idx].x = LV_COORD_MIN;
            scale_points[pt_idx].y = LV_COORD_MIN;
            pt_idx++;
        }
    }

    /* 
     * Create ONE unified scale wireframe element spanning the canvas 
     */
    lv_obj_t *scale_wireframe = lv_line_create(parent);
    
    // Pass the static array pointer safely
    lv_line_set_points(scale_wireframe, scale_points, total_points);

    // Style the default global line baseline
    lv_obj_set_style_line_color(scale_wireframe, lv_palette_lighten(LV_PALETTE_GREY, 2), LV_PART_MAIN);
    lv_obj_set_style_line_width(scale_wireframe, 2, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(scale_wireframe, true, LV_PART_MAIN);

    // Cover full layout coordinates 
    lv_obj_set_pos(scale_wireframe, 0, 0);
    lv_obj_clear_flag(scale_wireframe, LV_OBJ_FLAG_CLICKABLE);
}



/* -------------------------------------------------- */
/* Scale labels                                       */
/* -------------------------------------------------- */

static void create_heel_labels(lv_obj_t *parent)
{
    static const struct {
        const char *text;
        float angle;
    } labels[] = {
        { "-45", -45.0f },
        { "-30", -30.0f },
        { "-15", -15.0f },
        { " 0",   0.0f },
        { "15",   15.0f },
        { "30",   30.0f },
        { "45",   45.0f }
    };

    for (unsigned i = 0;
         i < sizeof(labels) / sizeof(labels[0]);
         ++i) {

        const lv_point_precise_t p =
            heel_point(
                labels[i].angle,
                HEEL_SCALE_LABEL_OUTER);

        lv_obj_t *label =
            lv_label_create(parent);

        lv_label_set_text(
            label,
            labels[i].text);

        lv_obj_set_style_text_font(
            label,
            &lv_font_montserrat_30,
            LV_PART_MAIN);

        lv_obj_set_style_text_color(
            label,
            lv_color_white(),
            LV_PART_MAIN);

        /*
         * Position the label around the scale.
         */
        lv_obj_set_pos(
            label,
            (lv_coord_t)p.x - 20,
            (lv_coord_t)p.y - 15);

        lv_obj_clear_flag(
            label,
            LV_OBJ_FLAG_CLICKABLE);
    }
}


/* -------------------------------------------------- */
/* Center hub                                         */
/* -------------------------------------------------- */

static void create_heel_center(lv_obj_t *parent)
{
    lv_obj_t *hub =
        lv_obj_create(parent);

    lv_obj_remove_style_all(
        hub);

    lv_obj_set_size(
        hub,
        24,
        24);

    lv_obj_set_style_radius(
        hub,
        LV_RADIUS_CIRCLE,
        LV_PART_MAIN);

    lv_obj_set_style_bg_color(
        hub,
        lv_palette_main(
            LV_PALETTE_ORANGE),
        LV_PART_MAIN);

    lv_obj_set_style_bg_opa(
        hub,
        LV_OPA_COVER,
        LV_PART_MAIN);

    lv_obj_set_pos(
        hub,
        HEEL_CENTER_X - 12,
        HEEL_CENTER_Y - 12);

    lv_obj_clear_flag(
        hub,
        LV_OBJ_FLAG_CLICKABLE);
}


/* -------------------------------------------------- */
/* Destroy                                            */
/* -------------------------------------------------- */

static void heel_destroy_cb(
    lv_updatable_screen_t *scr)
{
    (void)scr;

    /*
     * Deleting heel_display also deletes:
     *
     *   heel_scale
     *   ticks
     *   scale labels
     *   needle
     *   center hub
     */
    if (heel_display) {
        lv_obj_delete(
            heel_display);

        heel_display = nullptr;
    }

    /*
     * These objects belong directly to the screen.
     */
    if (pitch_label) {
        lv_obj_delete(
            pitch_label);

        pitch_label = nullptr;
    }

    if (heel_leeway_label) {
        lv_obj_delete(
            heel_leeway_label);

        heel_leeway_label = nullptr;
    }

    if (heel_drift_label) {
        lv_obj_delete(
            heel_drift_label);

        heel_drift_label = nullptr;
    }

    if (heel_set_label) {
        lv_obj_delete(
            heel_set_label);

        heel_set_label = nullptr;
    }

    if (heel_main_label) {
        lv_obj_delete(
            heel_main_label);

        heel_main_label = nullptr;
    }

    heel_scale = nullptr;
    heel_needle = nullptr;
}


/* -------------------------------------------------- */
/* UI Creation                                        */
/* -------------------------------------------------- */

static void lv_heel_display(
    lv_updatable_screen_t *scr)
{
    lv_obj_t *parent =
        scr->screen;


    /* ------------------------------------------------
     * Main display
     * ------------------------------------------------ */

    heel_display =
        lv_obj_create(parent);

    lv_obj_remove_style_all(
        heel_display);

    lv_obj_set_size(
        heel_display,
        HEEL_DISPLAY_SIZE,
        HEEL_DISPLAY_SIZE);

    /*
     * The 680x680 display is positioned so that the
     * heel scale occupies the lower portion of the
     * physical screen.
     */
    lv_obj_align(
        heel_display,
        LV_ALIGN_CENTER,
        0,
        -40);

    lv_obj_set_style_bg_opa(
        heel_display,
        LV_OPA_TRANSP,
        LV_PART_MAIN
    );

    lv_obj_clear_flag(
        heel_display,
        LV_OBJ_FLAG_CLICKABLE);


    /* ------------------------------------------------
     * Scale
     * ------------------------------------------------ */

    heel_scale =
        lv_obj_create(
            heel_display);

    lv_obj_remove_style_all(
        heel_scale);

    lv_obj_set_size(
        heel_scale,
        HEEL_DISPLAY_SIZE,
        HEEL_DISPLAY_SIZE);

    lv_obj_center(
        heel_scale);

    lv_obj_clear_flag(
        heel_scale,
        LV_OBJ_FLAG_CLICKABLE);

    create_heel_scale(
        heel_scale);

    create_heel_labels(
        heel_scale);


    /* ------------------------------------------------
     * Needle
     * ------------------------------------------------ */

    heel_needle =
        lv_line_create(
            heel_display);

    lv_obj_set_style_line_color(
        heel_needle,
        lv_palette_main(
            LV_PALETTE_ORANGE),
        LV_PART_MAIN);

    lv_obj_set_style_line_width(
        heel_needle,
        7,
        LV_PART_MAIN);

    lv_obj_set_style_line_rounded(
        heel_needle,
        true,
        LV_PART_MAIN);

    lv_obj_clear_flag(
        heel_needle,
        LV_OBJ_FLAG_CLICKABLE);

    /*
     * Points use heel_display coordinates.
     */
    lv_obj_set_pos(
        heel_needle,
        0,
        0);

    set_heel_value(
        0.0f);

    create_heel_center(
        heel_display);


    /* ------------------------------------------------
     * Pitch
     * ------------------------------------------------ */

    pitch_label =
        lv_label_create(parent);

    lv_obj_align(
        pitch_label,
        LV_ALIGN_TOP_LEFT,
        5,
        5);

    lv_obj_set_style_text_font(
        pitch_label,
        &lv_font_montserrat_30,
        LV_PART_MAIN);

    lv_label_set_text_static(
        pitch_label,
        "Pitch:   --");


    /* ------------------------------------------------
     * Leeway
     * ------------------------------------------------ */

    heel_leeway_label =
        lv_label_create(parent);

    lv_obj_align(
        heel_leeway_label,
        LV_ALIGN_TOP_LEFT,
        5,
        50);

    lv_obj_set_style_text_font(
        heel_leeway_label,
        &lv_font_montserrat_30,
        LV_PART_MAIN);

    lv_label_set_text_static(
        heel_leeway_label,
        "Leeway\n(est):\n--");


    /* ------------------------------------------------
     * Drift
     * ------------------------------------------------ */

    heel_drift_label =
        lv_label_create(parent);

    lv_obj_align(
        heel_drift_label,
        LV_ALIGN_TOP_LEFT,
        250,
        5);

    lv_obj_set_style_text_font(
        heel_drift_label,
        &lv_font_montserrat_30,
        LV_PART_MAIN);

    lv_label_set_text_static(
        heel_drift_label,
        "Drift (kt):  --");


    /* ------------------------------------------------
     * Set
     * ------------------------------------------------ */

    heel_set_label =
        lv_label_create(parent);

    lv_obj_align(
        heel_set_label,
        LV_ALIGN_TOP_LEFT,
        245,
        50);

    lv_obj_set_style_text_font(
        heel_set_label,
        &lv_font_montserrat_30,
        LV_PART_MAIN);

    lv_label_set_text_static(
        heel_set_label,
        "Set:\n--" LV_SYMBOL_DEGREES "");


    /* ------------------------------------------------
     * Center title
     * ------------------------------------------------ */

    heel_main_label =
        lv_label_create(parent);

    lv_obj_align(
        heel_main_label,
        LV_ALIGN_CENTER,
        0,
        -80);

    lv_obj_set_style_text_font(
        heel_main_label,
        &lv_font_montserrat_30,
        LV_PART_MAIN);

    lv_label_set_text_static(
        heel_main_label,
        "HEEL");
}


/* -------------------------------------------------- */
/* Screen Update                                      */
/* -------------------------------------------------- */

static void heel_update_cb(
    lv_updatable_screen_t *scr)
{
    (void)scr;

    if (!pitch_label ||
        !heel_leeway_label ||
        !heel_drift_label ||
        !heel_set_label ||
        !heel_needle) {
        return;
    }


    /* ------------------------------------------------
     * Pitch
     * ------------------------------------------------ */

    if (fresh(
            shipDataModel
                .navigation
                .attitude
                .pitch
                .age)) {

        lv_label_set_text_fmt(
            pitch_label,
            "Pitch:   %.1f" LV_SYMBOL_DEGREES,
            shipDataModel
                .navigation
                .attitude
                .pitch
                .deg);
    }
    else {

        lv_label_set_text_static(
            pitch_label,
            "Pitch:   --");
    }


    /* ------------------------------------------------
     * Leeway
     * ------------------------------------------------ */

    if (fresh(
            shipDataModel
                .navigation
                .leeway
                .age)) {

        lv_label_set_text_fmt(
            heel_leeway_label,
            "Leeway\n(est):\n%.1f" LV_SYMBOL_DEGREES,
            shipDataModel
                .navigation
                .leeway
                .deg);
    }
    else {

        lv_label_set_text_static(
            heel_leeway_label,
            "Leeway\n(est):\n--");
    }


    /* ------------------------------------------------
     * Drift
     * ------------------------------------------------ */

    if (fresh(
            shipDataModel
                .navigation
                .drift
                .age)) {

        lv_label_set_text_fmt(
            heel_drift_label,
            "Drift (kt):  %.1f",
            shipDataModel
                .navigation
                .drift
                .kn);
    }
    else {

        lv_label_set_text_static(
            heel_drift_label,
            "Drift (kt):  --");
    }


    /* ------------------------------------------------
     * Set
     * ------------------------------------------------ */

    if (fresh(
            shipDataModel
                .navigation
                .set_true
                .age)) {

        lv_label_set_text_fmt(
            heel_set_label,
            "Set:\n%.0f" LV_SYMBOL_DEGREES "",
            shipDataModel
                .navigation
                .set_true
                .deg);
    }
    else {

        lv_label_set_text_static(
            heel_set_label,
            "Set:\n--" LV_SYMBOL_DEGREES "");
    }


    /* ------------------------------------------------
     * Heel needle
     * ------------------------------------------------ */

    const float heel =
        fresh(
            shipDataModel
                .navigation
                .attitude
                .heel
                .age)
            ? shipDataModel
                  .navigation
                  .attitude
                  .heel
                  .deg
            : 0.0f;

    set_heel_value(
        heel);
}


/* -------------------------------------------------- */
/* Screen Definition                                  */
/* -------------------------------------------------- */

lv_updatable_screen_t heelScreen = {
    .screen = nullptr,
    .created = false,
    .create_cb = lv_heel_display,
    .update_cb = heel_update_cb,
    .destroy_cb = heel_destroy_cb
};


#ifdef __cplusplus
}
#endif