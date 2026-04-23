// app_startup.cpp
#include <runtime_loop.h>
#include <signalk_connection_manager.h>
#include <ui_depth.h>
#include <signalk_path_config.h>
#include <ui_speed.h>
#include <ui_manager.h>
#include <net_signalk_http.h>
#include <net_globals.h>

void start_application() {
    load_signalk_path_config();
    
    // Init chart histories on boot for background tracking
    depth_history = new ChartDataHistory("depth", get_signalk_path_config().depth_chart_duration, 300);
    speed_history = new ChartDataHistory("speed", get_signalk_path_config().speed_chart_duration, 300);
    
    signalk_path_config_web_begin();

    ui_manager_init();

    vTaskDelay(pdMS_TO_TICKS(2000));
    start_signalk_connection_task();

    app.onDelay(4000, []() {
        getVesselInfo();
    });

    run_main_loop(); 
}