#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
// #include "bsp_board_extra.h"
#include "time.h"
#include "string.h"
#include "esp_timer.h"
#include "Arduino.h"
#include <ArduinoJson.h>

// screen operations
#include "navigation.h"

// wifi setup
#include <WiFi.h>
#include <mdns.h>
#include "net_globals.h"
#include "keepalive.h"
#include "net_mdns.h"
#include "net_signalk_ws.h"

NetClient nmea0183Client;
NetClient skClient;
NetClient pypClient;

reactesp::ReactESP app;

#include "ship_data_model.h"
ship_data_t shipDataModel;
#include "ship_data_util.h"
#include "derived_data.h"

#include <WMM_Tinier.h>
WMM_Tinier myDeclination;

#include <TinyGPSPlus.h>

#include "signalk_parse.h"
#include "sunriset.h"
#include "hw_rtc.h"

#include "ui/ui_init.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "ui_settings_wifi.h"
#include "ui/ui_settings.h"
#include "net_signalk_http.h"

extern "C" void app_main()
{
    initArduino();  // initialize Arduino core
    ESP_ERROR_CHECK(nvs_flash_init());

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false,
        }
    };

    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    settingUpWiFi([]() {
        // delayed network discovery
    });

    bsp_display_lock(0);
    ui_manager_init();
    bsp_display_unlock();

    vTaskDelay(pdMS_TO_TICKS(2000));
    app.onRepeat(2000, []() {
        static bool connected = false;

        if (!connected) {
            bool found = discover_n_config();

            if (found) {
                String host = preferences.getString(SK_TCP_HOST_PREF);
                int port = preferences.getInt(SK_TCP_PORT_PREF);

                ESP_LOGI("WS", "Starting WS after discovery: %s:%d",
                        host.c_str(), port);

                signalk_ws_begin(host.c_str(), port);
                connected = true;
            }
        }
    });

    app.onDelay(4000, []() {
      getVesselInfo();
    });

    
    #define GO_SLEEP_TIMEOUT 1800000ul  // 30 minutes of inactivity before sleeping
    unsigned long last_ui_upd = 0;

    while(true) {
        app.tick();
        signalk_ws_loop();

        bsp_display_lock(0);
        ui_manager_update();
        bsp_display_unlock();
        vTaskDelay(pdMS_TO_TICKS(50));

        // if (!settingMode) {
        // if (last_touched > 0 && millis() - last_touched > GO_SLEEP_TIMEOUT) {
            // disconnect_clients();
            // save_page(current_index);
            // deep_sleep_with_touch_wakeup();
        // } else {
            // if (victron_mqtt_began) {
            //     victron_mqtt_client_loop(mqttClient);
            // }
            if ((millis() - last_ui_upd > 300) 
                // || (screens[page] == &clockScreen && millis() - last_ui_upd > 200)
            ) {  // throttle expensive UI updates, and calculations
                derive_data();
                last_ui_upd = millis();
            }
        // #ifdef ENABLE_SCREEN_SERVER
        //     // (not for production)
        //     if (detected) {
        //         screenServer0();
        //     }
        // #endif
            // }
        // }
    }  
}
