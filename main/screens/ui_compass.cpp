#include "ui_compass.h"

#include <navigation.h>
#include <ship_data_util.h>
#include <ui_init.h>

#include <cstdio>
#include <cmath>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================== */
/* Compass constants                                  */
/* ================================================== */

#define COMPASS_SIZE        680
#define COMPASS_CENTER      (COMPASS_SIZE / 2)
#define COMPASS_RADIUS      315

#define COMPASS_TICK_COUNT  72
#define COMPASS_MAJOR_COUNT 12

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


/* ================================================== */
/* LVGL objects                                       */
/* ================================================== */

static lv_obj_t *compass_display = nullptr;

static lv_obj_t *compass_l = nullptr;
static lv_obj_t *compass_hdt_l = nullptr;
static lv_obj_t *compass_cogt_l = nullptr;
static lv_obj_t *compass_mag_var_l = nullptr;

static lv_obj_t *labelNcont = nullptr;
static lv_obj_t *labelScont = nullptr;
static lv_obj_t *labelEcont = nullptr;
static lv_obj_t *labelWcont = nullptr;

static int16_t last_heading = -1;
static uint32_t last_compass_upd = 0;


/* ================================================== */
/* Helpers                                            */
/* ================================================== */

static int normalize_heading(int heading)
{
    while (heading < 0)
        heading += 360;

    while (heading >= 360)
        heading -= 360;

    return heading;
}


/* ================================================== */
/* Compass custom drawing                             */
/* ================================================== */

/*
 * LVGL 9 custom drawing callback.
 *
 * This replaces all of the old LVGL 8 canvas/meter
 * drawing code.
 *
 * No canvas.
 * No canvas buffer.
 * No lv_canvas_draw_line().
 * No lv_canvas_draw_text().
 */
static void compass_draw_event_cb(lv_event_t *e)
{
    if (!compass_display)
        return;

    lv_event_code_t code = lv_event_get_code(e);

    if (code != LV_EVENT_DRAW_MAIN)
        return;


    /*
     * Get the LVGL 9 drawing layer.
     */
    lv_layer_t *layer = lv_event_get_layer(e);

    if (!layer)
        return;


    /*
     * Get the compass object's absolute coordinates.
     *
     * Our drawing coordinates below are relative to
     * the compass object, so this allows the compass
     * to work regardless of where LVGL places it.
     */
    lv_area_t obj_area;

    lv_obj_get_coords(
        compass_display,
        &obj_area
    );


    const int32_t cx =
        (obj_area.x1 + obj_area.x2) / 2;

    const int32_t cy =
        (obj_area.y1 + obj_area.y2) / 2;


    /*
     * Get current heading.
     *
     * We draw the compass directly from the current
     * ship-data value rather than storing another
     * compass image buffer.
     */
    int heading =
        fresh(
            shipDataModel.navigation.heading_mag.age
        )
        ? (int)
            shipDataModel.navigation.heading_mag.deg
        : 0;

    heading = normalize_heading(heading);


    /*
     * Compass rotation.
     *
     * The compass card rotates opposite the vessel
     * heading so that the current heading remains
     * under the fixed pointer.
     */
    const float rotation =
        (360.0f - (float)heading) *
        (float)M_PI /
        180.0f;


    /* ================================================= */
    /* Tick marks                                        */
    /* ================================================= */

    lv_draw_line_dsc_t line_dsc;

    lv_draw_line_dsc_init(
        &line_dsc
    );

    line_dsc.color =
        lv_palette_main(LV_PALETTE_GREY);

    line_dsc.opa =
        LV_OPA_COVER;


    for (int i = 0;
         i < COMPASS_TICK_COUNT;
         ++i)
    {
        const float angle =
            rotation +
            (
                (float)i *
                360.0f /
                (float)COMPASS_TICK_COUNT
            ) *
            (float)M_PI /
            180.0f;


        /*
         * Every sixth tick is a 30-degree major tick.
         */
        const bool major =
            ((i % 6) == 0);


        const float outer_radius =
            (float)COMPASS_RADIUS;

        const float inner_radius =
            major
                ? (float)(COMPASS_RADIUS - 28)
                : (float)(COMPASS_RADIUS - 15);


        line_dsc.width =
            major ? 4 : 2;


        /*
         * LVGL 9 line descriptor supports p1/p2
         * with precise coordinates.
         */
        line_dsc.p1.x =
            cx +
            cosf(angle) *
            inner_radius;

        line_dsc.p1.y =
            cy +
            sinf(angle) *
            inner_radius;


        line_dsc.p2.x =
            cx +
            cosf(angle) *
            outer_radius;

        line_dsc.p2.y =
            cy +
            sinf(angle) *
            outer_radius;


        lv_draw_line(
            layer,
            &line_dsc
        );
    }


    /* ================================================= */
    /* Degree labels                                     */
    /* ================================================= */

    lv_draw_label_dsc_t label_dsc;

    lv_draw_label_dsc_init(
        &label_dsc
    );

    label_dsc.color =
        lv_color_white();

    label_dsc.font =
        &lv_font_montserrat_28;


    char text[8];


    for (int i = 0;
         i < COMPASS_MAJOR_COUNT;
         ++i)
    {
        const int degree =
            i * 30;


        const float angle =
            rotation +
            (float)degree *
            (float)M_PI /
            180.0f;


        const float label_radius =
            (float)(COMPASS_RADIUS - 52);


        const int32_t x =
            (int32_t)(
                cx +
                cosf(angle) *
                label_radius
            );


        const int32_t y =
            (int32_t)(
                cy +
                sinf(angle) *
                label_radius
            );


        snprintf(
            text,
            sizeof(text),
            "%d",
            degree
        );


        /*
         * Approximate label bounding box.
         */
        lv_area_t label_area;

        label_area.x1 =
            x - 28;

        label_area.y1 =
            y - 18;

        label_area.x2 =
            x + 28;

        label_area.y2 =
            y + 18;


        lv_draw_label(
            layer,
            &label_dsc,
            &label_area
        );
    }
}


/* ================================================== */
/* Cardinal direction labels                         */
/* ================================================== */

static lv_obj_t *create_cardinal_label(
    lv_obj_t *parent,
    const char *text,
    lv_color_t color)
{
    lv_obj_t *cont =
        lv_obj_create(parent);

    if (!cont)
        return nullptr;


    lv_obj_set_size(
        cont,
        42,
        42
    );


    lv_obj_set_style_pad_all(
        cont,
        2,
        LV_PART_MAIN
    );


    lv_obj_set_style_bg_color(
        cont,
        color,
        LV_PART_MAIN
    );


    lv_obj_set_style_bg_opa(
        cont,
        LV_OPA_COVER,
        LV_PART_MAIN
    );


    lv_obj_set_style_border_width(
        cont,
        0,
        LV_PART_MAIN
    );


    lv_obj_set_style_radius(
        cont,
        2,
        LV_PART_MAIN
    );


    /*
     * Initial position is directly above the
     * center of the compass.
     */
    lv_obj_align(
        cont,
        LV_ALIGN_CENTER,
        0,
        -48
    );


    lv_obj_t *label =
        lv_label_create(cont);

    if (label)
    {
        lv_obj_set_style_text_font(
            label,
            &lv_font_montserrat_32,
            0
        );

        lv_label_set_text_static(
            label,
            text
        );

        lv_obj_center(label);
    }


    /*
     * Rotation pivot is the center of the compass.
     */
    lv_obj_set_style_transform_pivot_x(
        cont,
        21,
        0
    );


    lv_obj_set_style_transform_pivot_y(
        cont,
        21 + 48,
        0
    );


    return cont;
}


/* ================================================== */
/* UI Creation                                        */
/* ================================================== */

static void lv_compass_display(
    lv_updatable_screen_t *scr)
{
    if (!scr)
        return;

    if (!scr->screen)
        return;


    /*
     * Main compass object.
     */
    compass_display =
        lv_obj_create(
            scr->screen
        );

    if (!compass_display)
        return;


    /*
     * Remove the normal object styling.
     */
    lv_obj_remove_style(
        compass_display,
        nullptr,
        LV_PART_MAIN
    );


    lv_obj_set_size(
        compass_display,
        COMPASS_SIZE,
        COMPASS_SIZE
    );


    lv_obj_center(
        compass_display
    );


    /*
     * Black background.
     */
    lv_obj_set_style_bg_color(
        compass_display,
        lv_color_black(),
        LV_PART_MAIN
    );


    lv_obj_set_style_bg_opa(
        compass_display,
        LV_OPA_COVER,
        LV_PART_MAIN
    );


    /*
     * We draw the compass ourselves.
     *
     * LVGL 9 will call compass_draw_event_cb()
     * whenever this object is rendered.
     */
    lv_obj_add_event_cb(
        compass_display,
        compass_draw_event_cb,
        LV_EVENT_DRAW_MAIN,
        nullptr
    );


    /*
     * The compass itself should not consume
     * touch gestures.
     */
    lv_obj_remove_flag(
        compass_display,
        LV_OBJ_FLAG_CLICKABLE
    );


    /* ================================================= */
    /* North                                             */
    /* ================================================= */

    labelNcont =
        create_cardinal_label(
            scr->screen,
            "N",
            lv_palette_main(
                LV_PALETTE_RED
            )
        );


    /* ================================================= */
    /* South                                             */
    /* ================================================= */

    labelScont =
        create_cardinal_label(
            scr->screen,
            "S",
            lv_palette_main(
                LV_PALETTE_BLUE
            )
        );


    /* ================================================= */
    /* East                                              */
    /* ================================================= */

    labelEcont =
        create_cardinal_label(
            scr->screen,
            "E",
            lv_palette_main(
                LV_PALETTE_GREY
            )
        );


    /* ================================================= */
    /* West                                              */
    /* ================================================= */

    labelWcont =
        create_cardinal_label(
            scr->screen,
            "W",
            lv_palette_main(
                LV_PALETTE_GREY
            )
        );


    /* ================================================= */
    /* Fixed heading marker                              */
    /* ================================================= */

    lv_obj_t *compass_mark_l =
        lv_label_create(
            scr->screen
        );

    if (compass_mark_l)
    {
        lv_label_set_text_static(
            compass_mark_l,
            LV_SYMBOL_DOWN
        );

        lv_obj_align(
            compass_mark_l,
            LV_ALIGN_CENTER,
            0,
            -100
        );
    }


    /* ================================================= */
    /* Center heading                                    */
    /* ================================================= */

    compass_l =
        lv_label_create(
            scr->screen
        );

    if (compass_l)
    {
        lv_obj_set_style_text_font(
            compass_l,
            &lv_font_montserrat_28,
            0
        );

        lv_label_set_text_static(
            compass_l,
            "--" LV_SYMBOL_DEGREES
        );

        lv_obj_center(
            compass_l
        );
    }


    /* ================================================= */
    /* HDT                                               */
    /* ================================================= */

    compass_hdt_l =
        lv_label_create(
            scr->screen
        );

    if (compass_hdt_l)
    {
        lv_label_set_text_static(
            compass_hdt_l,
            "HDT: --" LV_SYMBOL_DEGREES
        );

        lv_obj_align(
            compass_hdt_l,
            LV_ALIGN_TOP_LEFT,
            2,
            2
        );

        lv_obj_set_style_text_font(
            compass_hdt_l,
            &lv_font_montserrat_24,
            0
        );
    }


    /* ================================================= */
    /* COGT                                              */
    /* ================================================= */

    compass_cogt_l =
        lv_label_create(
            scr->screen
        );

    if (compass_cogt_l)
    {
        lv_label_set_text_static(
            compass_cogt_l,
            "COGT: --" LV_SYMBOL_DEGREES
        );

        lv_obj_align(
            compass_cogt_l,
            LV_ALIGN_TOP_RIGHT,
            -2,
            2
        );

        lv_obj_set_style_text_font(
            compass_cogt_l,
            &lv_font_montserrat_24,
            0
        );
    }


    /* ================================================= */
    /* Magnetic variation                                */
    /* ================================================= */

    compass_mag_var_l =
        lv_label_create(
            scr->screen
        );

    if (compass_mag_var_l)
    {
        lv_label_set_text_static(
            compass_mag_var_l,
            "Var:\n--" LV_SYMBOL_DEGREES
        );

        lv_obj_align(
            compass_mag_var_l,
            LV_ALIGN_BOTTOM_LEFT,
            2,
            -2
        );

        lv_obj_set_style_text_font(
            compass_mag_var_l,
            &lv_font_montserrat_24,
            0
        );
    }


    /*
     * Reset update state whenever the screen is
     * created.
     */
    last_heading = -1;
    last_compass_upd = 0;


    /*
     * Force an initial redraw.
     */
    lv_obj_invalidate(
        compass_display
    );
}


/* ================================================== */
/* Screen Update                                      */
/* ================================================== */

static void compass_update_cb(
    lv_updatable_screen_t *scr)
{
    (void)scr;


    if (!compass_display)
        return;


    const uint32_t now =
        lv_tick_get();


    /*
     * Update navigation information twice per second.
     */
    if ((now - last_compass_upd) < 500)
        return;


    last_compass_upd = now;


    /* ================================================= */
    /* Magnetic heading                                  */
    /* ================================================= */

    int heading =
        fresh(
            shipDataModel.navigation.heading_mag.age
        )
        ? (int)
            shipDataModel.navigation.heading_mag.deg
        : 0;


    heading =
        normalize_heading(
            heading
        );


    /* ================================================= */
    /* Heading changed                                   */
    /* ================================================= */

    if (heading != last_heading)
    {
        /*
         * The custom draw callback reads the current
         * shipDataModel heading whenever LVGL redraws.
         *
         * Invalidate the object so LVGL schedules a
         * redraw immediately.
         */
        lv_obj_invalidate(
            compass_display
        );


        /*
         * Rotate cardinal markers.
         */
        const int rot =
            normalize_heading(
                360 - heading
            );


        if (labelNcont)
        {
            lv_obj_set_style_transform_angle(
                labelNcont,
                rot * 10,
                0
            );
        }


        if (labelScont)
        {
            lv_obj_set_style_transform_angle(
                labelScont,
                (180 + rot) * 10,
                0
            );
        }


        if (labelEcont)
        {
            lv_obj_set_style_transform_angle(
                labelEcont,
                (90 + rot) * 10,
                0
            );
        }


        if (labelWcont)
        {
            lv_obj_set_style_transform_angle(
                labelWcont,
                (270 + rot) * 10,
                0
            );
        }


        /* ------------------------------------------ */
        /* Center heading                             */
        /* ------------------------------------------ */

        if (compass_l)
        {
            char buf[32];

            snprintf(
                buf,
                sizeof(buf),
                "%d" LV_SYMBOL_DEGREES,
                heading
            );

            lv_label_set_text(
                compass_l,
                buf
            );
        }


        last_heading =
            heading;
    }


    /* ================================================= */
    /* True heading                                      */
    /* ================================================= */

    if (
        compass_hdt_l &&
        fresh(
            shipDataModel.navigation.heading_true.age
        )
    )
    {
        char buf[32];

        snprintf(
            buf,
            sizeof(buf),
            "HDT: %d" LV_SYMBOL_DEGREES,
            (int)
                shipDataModel.navigation.heading_true.deg
        );

        lv_label_set_text(
            compass_hdt_l,
            buf
        );
    }


    /* ================================================= */
    /* True course over ground                           */
    /* ================================================= */

    if (
        compass_cogt_l &&
        fresh(
            shipDataModel.navigation.course_over_ground_true.age
        )
    )
    {
        char buf[32];

        snprintf(
            buf,
            sizeof(buf),
            "COGT: %d" LV_SYMBOL_DEGREES,
            (int)
                shipDataModel.navigation.course_over_ground_true.deg
        );

        lv_label_set_text(
            compass_cogt_l,
            buf
        );
    }


    /* ================================================= */
    /* Magnetic variation                                */
    /* ================================================= */

    if (
        compass_mag_var_l &&
        fresh(
            shipDataModel.navigation.mag_var.age
        )
    )
    {
        char buf[32];

        snprintf(
            buf,
            sizeof(buf),
            "Var:\n%.1f" LV_SYMBOL_DEGREES,
            shipDataModel.navigation.mag_var.deg
        );

        lv_label_set_text(
            compass_mag_var_l,
            buf
        );
    }
}


/* ================================================== */
/* Screen Init                                        */
/* ================================================== */

lv_updatable_screen_t compassScreen = {
    .screen = nullptr,
    .created = false,
    .create_cb = lv_compass_display,
    .update_cb = compass_update_cb
};


#ifdef __cplusplus
}
#endif