#include "ui_manager.h"
#include "screens/ui_engine.h"
#include "screens/ui_compass.h"
#include "screens/ui_gps.h"
#include "screens/ui_wind.h"

#include "navigation.h"

// Screens (extern C from each screen cpp)
extern lv_updatable_screen_t compassScreen;
extern lv_updatable_screen_t engineScreen;
extern lv_updatable_screen_t windScreen;
extern lv_updatable_screen_t gpsScreen;

// Currently active screen pointer
static lv_updatable_screen_t* currentScreen = NULL;

static lv_updatable_screen_t* screens[] =
{
    &engineScreen,
    &compassScreen,
    &gpsScreen,
    &windScreen
};

void ui_manager_load_screen(lv_updatable_screen_t *screen) {
    if (!screen || !screen->screen) return;

    if (currentScreen && currentScreen->screen) {
        lv_obj_clean(currentScreen->screen); // remove old content if needed
    }

    lv_scr_load(screen->screen);
    currentScreen = screen;
}

void ui_manager_init(void) {
    // Initialize all screens first
    init_engineScreen();
    init_compassScreen();
    init_gpsScreen();
    init_windScreen();

    // Initialize navigation (swipes)
    navigation_init(screens, sizeof(screens) / sizeof(screens[0]));

    // Load default screen
    ui_manager_load_screen(&engineScreen);
}

void ui_manager_update(void)
{
    // Call the current screen's update callback
    if(currentScreen && currentScreen->update_cb) {
        currentScreen->update_cb();
    }

    // Let LVGL do its processing (timers, animations, input)
    lv_timer_handler();

    // Small delay to prevent watchdog
    vTaskDelay(pdMS_TO_TICKS(5));
}