// system_init.cpp

#include "system_init.h"

#include "Arduino.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "bsp/esp32_p4_wifi6_touch_lcd_4b.h"


void init_system()
{
    /* ------------------------------------------------ */
    /* Arduino / Serial                                 */
    /* ------------------------------------------------ */

    initArduino();

    Serial.begin(115200);
    delay(100);

    Serial.println("[DEBUG] init_system: serial logging enabled");


    /* ------------------------------------------------ */
    /* NVS                                              */
    /* ------------------------------------------------ */

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGI("NVS", "Erasing NVS...");

        ESP_ERROR_CHECK(nvs_flash_erase());

        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);


    /* ------------------------------------------------ */
    /* Display / LVGL                                   */
    /* ------------------------------------------------ */

    Serial.println("[DEBUG] init_system: starting display");

    /*
     * The Waveshare ESP32-P4 BSP handles the LVGL
     * port configuration internally.
     *
     * Do NOT use:
     *
     *     ESP_LVGL_PORT_INIT_CONFIG()
     *
     * or construct bsp_display_cfg_t here.
     */

    bsp_display_start();

    Serial.println("[DEBUG] init_system: display started");


    /* ------------------------------------------------ */
    /* Backlight                                        */
    /* ------------------------------------------------ */

    bsp_display_backlight_on();

    Serial.println("[DEBUG] init_system: backlight on");
}