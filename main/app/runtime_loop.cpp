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


/* --------------------------------------------------------------------------
 * Network
 * -------------------------------------------------------------------------- */

static void process_network()
{
    signalk_ws_loop();
    signalk_path_config_web_loop();
}


/* --------------------------------------------------------------------------
 * UI
 * -------------------------------------------------------------------------- */

static void process_ui()
{
    /*
     * Process deferred brightness outside the LVGL lock.
     */
    navigation_process_deferred_brightness();


    /*
     * Screen creation and loading acquire their own LVGL locks.
     */
    ui_manager_process_deferred_screen_creation();

    ui_manager_process_deferred_screen_load();


    /*
     * ui_manager_update() acquires the LVGL lock itself.
     *
     * DO NOT wrap this in another display lock.
     */
    ui_manager_update();
}


/* --------------------------------------------------------------------------
 * Data
 * -------------------------------------------------------------------------- */

static void process_data()
{
    derive_data();

    last_ui_upd =
        millis();
}


/* --------------------------------------------------------------------------
 * Charts
 * -------------------------------------------------------------------------- */

static void process_charts()
{
    /*
     * Queue chart data globally for background collection.
     */
    depth_queue_chart_data();

    speed_queue_chart_data();


    /*
     * Process deferred chart updates.
     */
    depth_process_deferred_chart_updates();

    speed_process_deferred_chart_updates();
}


/* --------------------------------------------------------------------------
 * Main loop
 * -------------------------------------------------------------------------- */

void run_main_loop()
{
    while(true)
    {
        app.tick();

        process_network();

        process_ui();

        process_charts();


        /*
         * Throttle expensive data calculations.
         */
        if(
            millis() - last_ui_upd > 300
        )
        {
            process_data();
        }


        /*
         * 20 ms gives responsive swipe handling.
         */
        vTaskDelay(
            pdMS_TO_TICKS(20)
        );
    }
}