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
    // Signal K path configuration
    // -------------------------------------------------------------------------

    load_signalk_path_config();


    // -------------------------------------------------------------------------
    // Chart histories
    //
    // Initialize these before entering the runtime loop.
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
    // Web configuration server
    //
    // This must be available independently of Signal K discovery.
    // -------------------------------------------------------------------------

    signalk_path_config_web_begin();


    // -------------------------------------------------------------------------
    // UI
    // -------------------------------------------------------------------------

    ui_manager_init();


    // -------------------------------------------------------------------------
    // Signal K connection manager
    //
    // Discovery now runs in its own FreeRTOS task.
    //
    // DO NOT add a blocking delay or getVesselInfo() call here.
    // -------------------------------------------------------------------------

    start_signalk_connection_task();


    // -------------------------------------------------------------------------
    // Enter the normal application loop.
    //
    // The web UI remains responsive while Signal K discovery is running.
    // -------------------------------------------------------------------------

    run_main_loop();
}