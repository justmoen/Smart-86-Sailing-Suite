// main/net/net_globals.cpp
#define WEBSOCKETS_DEBUG 1

#include "net_globals.h"
#include "net_signalk_ws.h"
#include "signalk_path_config.h"
#include "esp_log.h"

static const char* TAG = "NET_GLOBALS";

// Track the task handle if you ever need to inspect or suspend it
static TaskHandle_t SignalKNetTaskHandle = NULL;

/**
 * @brief Thread runner for the network processing engine
 */
static void signalk_network_task(void * pvParameters)
{
    ESP_LOGI(TAG, "Signal K background network task running on Core 0");

    while(true)
    {
        // Continuously tick the WebSocket client stack
        signalk_ws_loop();
        
        // Continuously tick the path configuration engine
        signalk_path_config_web_loop();

        // Delay for a tight 10ms window. 
        // This yields execution control to allow the underlying ESP-IDF Wi-Fi stack 
        // to breathe while maintaining near-instant ingestion of incoming JSON frames.
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void start_network_processing()
{
    if (SignalKNetTaskHandle != NULL) {
        return;
    }

    xTaskCreatePinnedToCore(
        signalk_network_task,
        "SK_Net_Loop",
        4096,
        NULL,
        1, // Lower priority to 1 so the ESP-IDF Wi-Fi/TCP stack (Priority 18-20) remains completely unhindered
        &SignalKNetTaskHandle,
        0  // Pin strictly to CPU Core 0
    );
}