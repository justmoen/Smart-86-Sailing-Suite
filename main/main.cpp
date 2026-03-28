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
#include "bsp_board_extra.h"
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

#include "ship_data_model.h"
ship_data_t shipDataModel;
#include "ship_data_util.h"

#include <WMM_Tinier.h>
WMM_Tinier myDeclination;

#include <TinyGPSPlus.h>

#include "signalk_parse.h"
#include "sunriset.h"
#include "hw_rtc.h"

#include "ui_theme.h"
#include "ui_manager.h"
#include "ui_settings_wifi.h"
#include "net/net_mdns.h"

extern "C" void app_main(void)
{
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

    vTaskDelay(pdMS_TO_TICKS(1000));
    discover_n_config();

    while(true)
    {
        bsp_display_lock(0);
        ui_manager_update();
        bsp_display_unlock();

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}