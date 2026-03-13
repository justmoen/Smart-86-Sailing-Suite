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

void ui_manager_init(void)
{
    // Initialize each screen only once
    if(compassScreen.screen == NULL) init_compassScreen();
    if(engineScreen.screen  == NULL) init_engineScreen();
    if(windScreen.screen    == NULL) init_windScreen();
    if(gpsScreen.screen    == NULL) init_gpsScreen();

    // Load default screen
    ui_manager_load_screen(&windScreen);
}

void ui_manager_load_screen(lv_updatable_screen_t* screen)
{
    if(screen == NULL || screen->screen == NULL) return;

    currentScreen = screen;
    lv_scr_load(screen->screen);
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