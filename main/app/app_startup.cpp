// app_startup.cpp

#include <runtime_loop.h>
#include <signalk_connection_manager.h>

#include <ui_depth.h>
#include <signalk_path_config.h>
#include <ui_speed.h>
#include <ui_manager.h>

#include <net_signalk_http.h>
#include <net_globals.h>


void start_application()
{
    // -------------------------------------------------------------------------
    // Load application configuration.
    // -------------------------------------------------------------------------

    load_signalk_path_config();


    // -------------------------------------------------------------------------
    // Initialize chart histories.
    // -------------------------------------------------------------------------

    depth_history =
        new ChartDataHistory(
            "depth",
            get_signalk_path_config().depth_chart_duration,
            300);

    speed_history =
        new ChartDataHistory(
            "speed",
            get_signalk_path_config().speed_chart_duration,
            300);


    // -------------------------------------------------------------------------
    // Start the web configuration server and UI immediately.
    //
    // Do not delay startup here. Signal K discovery happens in its own
    // FreeRTOS task.
    // -------------------------------------------------------------------------

    signalk_path_config_web_begin();

    ui_manager_init();


    // -------------------------------------------------------------------------
    // Start Signal K discovery/connection manager.
    //
    // Discovery, mDNS, DNS, and vessel HTTP requests all run outside the
    // main application loop.
    // -------------------------------------------------------------------------

    start_signalk_connection_task();


    // -------------------------------------------------------------------------
    // Enter the main application loop.
    // -------------------------------------------------------------------------

    run_main_loop();
}