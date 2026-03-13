#include "ui_manager.h"
#include "screens/ui_engine.h"
#include "screens/ui_compass.h"
#include "screens/ui_gps.h"
#include "screens/ui_wind.h"

#include "navigation.h"

static lv_updatable_screen_t* screens[] =
{
    &engineScreen,
    &compassScreen,
    &gpsScreen,
    &windScreen
};

static const int screen_count = sizeof(screens) / sizeof(screens[0]);

static int active_screen = 0;

void ui_manager_init()
{
    init_engineScreen();
    init_compassScreen();
    init_gpsScreen();
    init_windScreen();

    for(int i=0;i<screen_count;i++)
    {
        if(screens[i]->init_cb)
            screens[i]->init_cb(screens[i]->screen);
    }

    lv_scr_load(screens[active_screen]->screen);
}

void ui_manager_update()
{
    for(int i=0;i<screen_count;i++)
    {
        if(screens[i]->update_cb)
            screens[i]->update_cb();
    }
}

void ui_next_screen()
{
    active_screen++;

    if(active_screen >= screen_count)
        active_screen = 0;

    lv_scr_load(screens[active_screen]->screen);
}