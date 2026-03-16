#include "navigation.h"

#include "ui_manager.h"
#include "screens/ui_engine.h"
#include "screens/ui_compass.h"
#include "screens/ui_gps.h"
#include "screens/ui_wind.h"

static lv_updatable_screen_t* screens[] = {
    &engineScreen,
    &compassScreen,
    &windScreen
};

static const int screen_count =
    sizeof(screens) / sizeof(screens[0]);

static int current_index = 0;
static lv_updatable_screen_t *current_screen = nullptr;

static void create_if_needed(lv_updatable_screen_t *scr)
{
    if (!scr->created)
    {
        scr->screen = lv_obj_create(NULL);

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

    current_index = 0;
    ui_manager_show(screens[0]);
}

void ui_manager_update()
{
    if (!current_screen) return;

    if (current_screen->update_cb)
        current_screen->update_cb(current_screen);
}