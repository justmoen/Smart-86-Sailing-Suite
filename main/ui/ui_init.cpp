#include "ui_init.h"
#include <ui_screens.h>

void apply_screen_style(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);
}

void apply_meter_style(lv_obj_t *meter)
{
    lv_obj_set_style_bg_color(meter, lv_palette_lighten(LV_PALETTE_GREY, 2), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(meter, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_border_width(meter, 0, LV_PART_MAIN);
}