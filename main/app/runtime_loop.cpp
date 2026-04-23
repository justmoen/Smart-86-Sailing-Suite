// runtime_loop.cpp
#include <net_globals.h>
#include <net_signalk_ws.h>
#include <navigation.h>
#include <ui_manager.h>
#include <ui_depth.h>
#include <ui_speed.h>
#include <signalk_path_config.h>
#include "bsp/esp-bsp.h"
#include <derived_data.h>

unsigned long last_ui_upd = 0;

static void process_network() {
    signalk_ws_loop();
    signalk_path_config_web_loop();
}

static void process_ui() {
    // Process deferred operations OUTSIDE display lock
    navigation_process_deferred_brightness();
    ui_manager_process_deferred_screen_creation();  // Create screens first
    ui_manager_process_deferred_screen_load();      // Then load them

    bsp_display_lock(0);
    ui_manager_update();
    bsp_display_unlock();
}

static void process_data() {
    derive_data();
    last_ui_upd = millis();
}

static void process_charts() {
    // Queue chart data globally for background collection
    depth_queue_chart_data();
    speed_queue_chart_data();
    
    depth_process_deferred_chart_updates();          // Update depth chart (history/charts if screen active)
    speed_process_deferred_chart_updates();          // Update speed chart (history/charts if screen active)
}

void run_main_loop() {
    while(true) {
        app.tick();
        process_network();
        process_ui();
        process_charts();

        if ((millis() - last_ui_upd > 300) 
            // || (screens[page] == &clockScreen && millis() - last_ui_upd > 200)
        ) {  // throttle expensive UI updates, and calculations
            process_data();
        }
        vTaskDelay(pdMS_TO_TICKS(20));  // Reduced from 50ms for snappier swipe response
    } 
}