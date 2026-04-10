#include "navigation.h"

#include "ui_manager.h"
#include "screens/ui_wind.h"
#include "screens/ui_engine.h"
#include "screens/ui_depth.h"
#include "screens/ui_speed.h"
// #include "screens/ui_compass.h"
// #include "screens/ui_gps.h"
#include "screens/ui_tanks.h"
#include "screens/ui_heel.h"
#include "screens/ui_reboot.h"

static lv_updatable_screen_t* screens[] = {
    &windScreen,
    &engineScreen,
    &depthScreen,
    &speedScreen,
    // &compassScreen, // needs work
    // &gpsScreen, // needs work
    &tanksScreen,
    &heelScreen,
    &rebootScreen
};

static const int screen_count =
    sizeof(screens) / sizeof(screens[0]);

int current_index = 0;
static lv_updatable_screen_t *current_screen = nullptr;

static void create_if_needed(lv_updatable_screen_t *scr)
{
    if (!scr->created)
    {
        scr->screen = lv_obj_create(NULL);
        apply_screen_style(scr->screen);

        lv_obj_add_event_cb(scr->screen, gesture_event_cb, LV_EVENT_ALL, NULL);

        if (scr->create_cb)
            scr->create_cb(scr);

        scr->created = true;
    }
}

void ui_manager_show(lv_updatable_screen_t *scr)
{
    if (!scr) return;

    create_if_needed(scr);

    current_screen = scr;

    save_last_screen(current_index);
    lv_scr_load(scr->screen);
}

void ui_manager_next()
{
    current_index++;
    if (current_index >= screen_count)
        current_index = 0;

    ui_manager_show(screens[current_index]);
}

void ui_manager_prev()
{
    current_index--;
    if (current_index < 0)
        current_index = screen_count - 1;

    ui_manager_show(screens[current_index]);
}

void ui_manager_init()
{
    for (int i = 0; i < screen_count; i++) {
        screens[i]->created = false;
        screens[i]->screen = nullptr;
    }

    current_index = load_last_screen();

    if (current_index < 0 || current_index >= screen_count) {
        current_index = 0;
    }
    default_settings();
    ui_manager_show(screens[current_index]);
}

void ui_manager_update()
{
    if (!current_screen) return;

    if (current_screen->update_cb)
        current_screen->update_cb(current_screen);
}