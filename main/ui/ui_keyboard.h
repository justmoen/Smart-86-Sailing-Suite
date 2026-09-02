#ifndef UI_KEYBOARD_H
#define UI_KEYBOARD_H

#include <lvgl.h>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================== */
/* Keyboard state                                     */
/* ================================================== */

static lv_obj_t *kb = nullptr;

static int keybd_index = 0;


/* ================================================== */
/* Button control map                                 */
/*                                                   */
/* LVGL 9 renamed:                                   */
/*   lv_btnmatrix_ctrl_t                              */
/*       -> lv_buttonmatrix_ctrl_t                    */
/*                                                   */
/* Width values are relative button widths.          */
/* ================================================== */

static const lv_buttonmatrix_ctrl_t ctrl_map[] = {
    LV_BUTTONMATRIX_CTRL_WIDTH_4,
    LV_BUTTONMATRIX_CTRL_WIDTH_4,
    LV_BUTTONMATRIX_CTRL_WIDTH_4,

    LV_BUTTONMATRIX_CTRL_WIDTH_4,
    LV_BUTTONMATRIX_CTRL_WIDTH_4,
    LV_BUTTONMATRIX_CTRL_WIDTH_4,

    LV_BUTTONMATRIX_CTRL_WIDTH_4,
    LV_BUTTONMATRIX_CTRL_WIDTH_4,
    LV_BUTTONMATRIX_CTRL_WIDTH_4,

    LV_BUTTONMATRIX_CTRL_WIDTH_3,
    LV_BUTTONMATRIX_CTRL_WIDTH_3,
    LV_BUTTONMATRIX_CTRL_WIDTH_3,
    LV_BUTTONMATRIX_CTRL_WIDTH_3
};


/* ================================================== */
/* Keyboard maps                                      */
/* ================================================== */

/*
 * LVGL keyboard maps are terminated with NULL in LVGL 9.
 *
 * "\n" starts a new row.
 *
 * The special symbols retain their standard keyboard
 * behavior:
 *
 *   LV_SYMBOL_OK
 *   LV_SYMBOL_BACKSPACE
 *   LV_SYMBOL_LEFT
 *   LV_SYMBOL_RIGHT
 */

static const char *btnm_mapplus[11][23] = {

    /* 0 - lower a-i */
    {
        "a", "b", "c", "\n",
        "d", "e", "f", "\n",
        "g", "h", "i", "\n",
        LV_SYMBOL_OK,
        LV_SYMBOL_BACKSPACE,
        LV_SYMBOL_LEFT,
        LV_SYMBOL_RIGHT,
        ""
    },

    /* 1 - lower j-r */
    {
        "j", "k", "l", "\n",
        "m", "n", "o", "\n",
        "p", "q", "r", "\n",
        LV_SYMBOL_OK,
        LV_SYMBOL_BACKSPACE,
        LV_SYMBOL_LEFT,
        LV_SYMBOL_RIGHT,
        ""
    },

    /* 2 - lower s-z */
    {
        "s", "t", "u", "\n",
        "v", "w", "x", "\n",
        "y", "z", " ", "\n",
        LV_SYMBOL_OK,
        LV_SYMBOL_BACKSPACE,
        LV_SYMBOL_LEFT,
        LV_SYMBOL_RIGHT,
        ""
    },

    /* 3 - upper A-I */
    {
        "A", "B", "C", "\n",
        "D", "E", "F", "\n",
        "G", "H", "I", "\n",
        LV_SYMBOL_OK,
        LV_SYMBOL_BACKSPACE,
        LV_SYMBOL_LEFT,
        LV_SYMBOL_RIGHT,
        ""
    },

    /* 4 - upper J-R */
    {
        "J", "K", "L", "\n",
        "N", "M", "O", "\n",
        "P", "Q", "R", "\n",
        LV_SYMBOL_OK,
        LV_SYMBOL_BACKSPACE,
        LV_SYMBOL_LEFT,
        LV_SYMBOL_RIGHT,
        ""
    },

    /* 5 - upper S-Z */
    {
        "S", "T", "U", "\n",
        "V", "W", "X", "\n",
        "Y", "Z", " ", "\n",
        LV_SYMBOL_OK,
        LV_SYMBOL_BACKSPACE,
        LV_SYMBOL_LEFT,
        LV_SYMBOL_RIGHT,
        ""
    },

    /* 6 - numbers */
    {
        "1", "2", "3", "\n",
        "4", "5", "6", "\n",
        "7", "8", "9", "\n",
        LV_SYMBOL_OK,
        LV_SYMBOL_BACKSPACE,
        LV_SYMBOL_LEFT,
        LV_SYMBOL_RIGHT,
        ""
    },

    /* 7 - math / punctuation */
    {
        "0", "+", "-", "\n",
        "/", "*", "=", "\n",
        "!", "?", " ", "\n",
        LV_SYMBOL_OK,
        LV_SYMBOL_BACKSPACE,
        LV_SYMBOL_LEFT,
        LV_SYMBOL_RIGHT,
        ""
    },

    /* 8 - brackets */
    {
        "<", ">", "@", "\n",
        "%", "$", "(", "\n",
        ")", "{", "}", "\n",
        LV_SYMBOL_OK,
        LV_SYMBOL_BACKSPACE,
        LV_SYMBOL_LEFT,
        LV_SYMBOL_RIGHT,
        ""
    },

    /* 9 - punctuation */
    {
        "[", "]", ";", "\n",
        "\"", "'", ".", "\n",
        ",", ":", " ", "\n",
        LV_SYMBOL_OK,
        LV_SYMBOL_BACKSPACE,
        LV_SYMBOL_LEFT,
        LV_SYMBOL_RIGHT,
        ""
    },

    /* 10 - miscellaneous */
    {
        "\\", "_", "~", "\n",
        "|", "&", "^", "\n",
        "`", "#", " ", "\n",
        LV_SYMBOL_OK,
        LV_SYMBOL_BACKSPACE,
        LV_SYMBOL_LEFT,
        LV_SYMBOL_RIGHT,
        ""
    }
};


/* ================================================== */
/* Number of custom keyboard maps                     */
/* ================================================== */

static constexpr size_t KEYBOARD_MAP_COUNT =
    sizeof(btnm_mapplus) / sizeof(btnm_mapplus[0]);


/* ================================================== */
/* Keyboard event handler                             */
/* ================================================== */

static void kb_event_cb(lv_event_t *e)
{
    if (!e)
        return;

    lv_event_code_t code = lv_event_get_code(e);

    if (code != LV_EVENT_VALUE_CHANGED)
        return;

    /*
     * LVGL 9:
     *
     * lv_event_get_target_obj()
     *
     * is preferred over lv_event_get_target() for
     * widget targets when compiling as C++.
     */
    lv_obj_t *keyboard =
        lv_event_get_target_obj(e);

    if (!keyboard)
        return;


    /*
     * LVGL 9 renamed:
     *
     * lv_keyboard_get_selected_btn()
     *     ->
     * lv_keyboard_get_selected_button()
     */
    uint32_t button =
        lv_keyboard_get_selected_button(keyboard);


    /*
     * LVGL 9 renamed:
     *
     * lv_keyboard_get_btn_text()
     *     ->
     * lv_keyboard_get_button_text()
     */
    const char *txt =
        lv_keyboard_get_button_text(
            keyboard,
            button
        );

    if (!txt)
        return;


    /* ---------------------------------------------- */
    /* Next keyboard page                             */
    /* ---------------------------------------------- */

    if (strcmp(LV_SYMBOL_RIGHT, txt) == 0)
    {
        keybd_index++;

        if (keybd_index >= (int)KEYBOARD_MAP_COUNT)
            keybd_index = 0;

        lv_keyboard_set_map(
            keyboard,
            LV_KEYBOARD_MODE_TEXT_LOWER,
            btnm_mapplus[keybd_index],
            ctrl_map
        );

        /*
         * Do not allow the default keyboard event
         * handler to process the navigation button.
         */
        lv_event_stop_processing(e);

        return;
    }


    /* ---------------------------------------------- */
    /* Previous keyboard page                         */
    /* ---------------------------------------------- */

    if (strcmp(LV_SYMBOL_LEFT, txt) == 0)
    {
        keybd_index--;

        if (keybd_index < 0)
            keybd_index = (int)KEYBOARD_MAP_COUNT - 1;

        lv_keyboard_set_map(
            keyboard,
            LV_KEYBOARD_MODE_TEXT_LOWER,
            btnm_mapplus[keybd_index],
            ctrl_map
        );

        /*
         * Prevent the normal keyboard handler from
         * moving the textarea cursor.
         */
        lv_event_stop_processing(e);

        return;
    }
}


/* ================================================== */
/* Create custom keyboard                             */
/* ================================================== */

static lv_obj_t *lv_keyboard2(lv_obj_t *parent)
{
    /*
     * Create keyboard.
     */
    lv_obj_t *keyboard =
        lv_keyboard_create(parent);

    if (!keyboard)
        return nullptr;


    /*
     * Install our first custom map.
     *
     * LVGL 9 uses lv_buttonmatrix_ctrl_t internally,
     * exposed through lv_keyboard_set_map().
     */
    lv_keyboard_set_map(
        keyboard,
        LV_KEYBOARD_MODE_TEXT_LOWER,
        btnm_mapplus[0],
        ctrl_map
    );


    /*
     * Size.
     */
    lv_obj_set_height(
        keyboard,
        (LV_VER_RES / 2) + 5
    );

    lv_obj_set_width(
        keyboard,
        LV_HOR_RES - 4
    );


    /*
     * Keyboard button font.
     */
    lv_obj_set_style_text_font(
        keyboard,
        &lv_font_montserrat_32,
        LV_PART_ITEMS
    );


    /*
     * Remove LVGL's default event handler so our
     * navigation buttons can be handled explicitly.
     */
    lv_obj_remove_event_cb(
        keyboard,
        lv_keyboard_def_event_cb
    );


    /*
     * Our custom handler.
     */
    lv_obj_add_event_cb(
        keyboard,
        kb_event_cb,
        LV_EVENT_VALUE_CHANGED,
        nullptr
    );


    /*
     * Restore the normal keyboard handler for all
     * normal keyboard functionality.
     *
     * Our handler calls lv_event_stop_processing()
     * for LEFT/RIGHT, so those buttons don't reach
     * the default handler.
     */
    lv_obj_add_event_cb(
        keyboard,
        lv_keyboard_def_event_cb,
        LV_EVENT_VALUE_CHANGED,
        nullptr
    );


    /*
     * Keep the global keyboard pointer synchronized
     * with the object actually returned to the caller.
     */
    kb = keyboard;

    return keyboard;
}


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* UI_KEYBOARD_H */