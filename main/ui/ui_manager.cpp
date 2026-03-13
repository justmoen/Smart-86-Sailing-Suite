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

lv_updatable_screen_t *activeScreen = nullptr;

void ui_manager_init()
{
    init_engineScreen();
    init_compassScreen();
    init_gpsScreen();
    init_windScreen();

    navigation_init(screens, sizeof(screens) / sizeof(screens[0]));

    // Set the initial screen
    activeScreen = &engineScreen;
    lv_scr_load(activeScreen->screen);

    // Call the init callback to populate objects
    if (activeScreen->init_cb) {
        activeScreen->init_cb(activeScreen->screen);
    }
}

void ui_manager_update() {
    if (!activeScreen) return;

    // Call LVGL task handler
    lv_task_handler();

    // Call the screen-specific update
    if (activeScreen->update_cb) {
        activeScreen->update_cb();
    }
}