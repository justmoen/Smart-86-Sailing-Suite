// signalk_connection_manager.cpp

#include <net_globals.h>

#include "net_mdns.h"
#include "net_signalk_ws.h"
#include "net_signalk_http.h"

#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "SK_MANAGER";

static TaskHandle_t discovery_task_handle = nullptr;

static volatile bool discovery_finished = false;
static volatile bool discovery_found = false;

static bool websocket_started = false;
static bool vessel_info_started = false;


// -----------------------------------------------------------------------------
// SIGNAL K DISCOVERY TASK
//
// mDNS and DNS discovery are deliberately performed outside the main
// ReactESP/LVGL/web-server loop.
//
// discover_n_config() can block while waiting for mDNS/DNS responses.
// That is acceptable here because this task is independent of the main
// application loop.
// -----------------------------------------------------------------------------

static void signalKDiscoveryTask(void *parameter)
{
    (void)parameter;

    ESP_LOGI(
        TAG,
        "Signal K discovery task started");


    // -------------------------------------------------------------------------
    // Keep trying until Signal K is discovered.
    //
    // A failed discovery attempt does NOT block the main application.
    // -------------------------------------------------------------------------

    while (!discovery_found) {

        ESP_LOGI(
            TAG,
            "Starting Signal K discovery attempt");


        bool found =
            discover_n_config();


        if (found) {

            ESP_LOGI(
                TAG,
                "Signal K discovered successfully");

            discovery_found =
                true;

            discovery_finished =
                true;

            break;
        }


        ESP_LOGW(
            TAG,
            "Signal K discovery attempt failed");

        ESP_LOGI(
            TAG,
            "Retrying Signal K discovery in 10 seconds");


        vTaskDelay(
            pdMS_TO_TICKS(10000));
    }


    discovery_finished =
        true;

    discovery_task_handle =
        nullptr;


    ESP_LOGI(
        TAG,
        "Signal K discovery task finished");


    vTaskDelete(
        nullptr);
}


// -----------------------------------------------------------------------------
// VESSEL INFORMATION TASK
//
// getVesselInfo() performs several HTTP requests. Keep those requests away
// from the main application loop as well.
// -----------------------------------------------------------------------------

static void vesselInfoTask(void *parameter)
{
    (void)parameter;

    ESP_LOGI(
        TAG,
        "Vessel information task started");


    // Give the WebSocket/network stack a moment to initialize.
    vTaskDelay(
        pdMS_TO_TICKS(1000));


    getVesselInfo();


    ESP_LOGI(
        TAG,
        "Vessel information task finished");


    vTaskDelete(
        nullptr);
}


// -----------------------------------------------------------------------------
// START SIGNAL K CONNECTION MANAGER
// -----------------------------------------------------------------------------

void start_signalk_connection_task()
{
    ESP_LOGI(
        TAG,
        "Starting Signal K connection manager");


    // -------------------------------------------------------------------------
    // Start the discovery task exactly once.
    // -------------------------------------------------------------------------

    if (discovery_task_handle == nullptr &&
        !discovery_finished) {

        BaseType_t result =
            xTaskCreate(
                signalKDiscoveryTask,
                "sk_discovery",
                8192,
                nullptr,
                1,
                &discovery_task_handle);


        if (result != pdPASS) {

            ESP_LOGE(
                TAG,
                "FAILED to create Signal K discovery task");

            discovery_task_handle =
                nullptr;

            return;
        }


        ESP_LOGI(
            TAG,
            "Signal K discovery task created");
    }


    // -------------------------------------------------------------------------
    // Main-loop callback.
    //
    // IMPORTANT:
    //
    // This callback does NOT perform discovery.
    //
    // It only checks whether the background task has completed and, if so,
    // starts the WebSocket connection.
    // -------------------------------------------------------------------------

    app.onRepeat(
        200,
        []() {

            // -----------------------------------------------------------------
            // Start WebSocket after discovery succeeds.
            //
            // This section is intentionally short and non-blocking.
            // -----------------------------------------------------------------

            if (discovery_found &&
                !websocket_started) {

                Preferences prefs;


                if (!prefs.begin(
                        "signalk",
                        true)) {

                    ESP_LOGE(
                        TAG,
                        "FAILED to open Signal K preferences");

                    return;
                }


                String host =
                    prefs.getString(
                        SK_TCP_HOST_PREF,
                        "");

                int port =
                    prefs.getInt(
                        SK_TCP_PORT_PREF,
                        3000);


                prefs.end();


                if (host.length() == 0) {

                    ESP_LOGW(
                        TAG,
                        "Discovery completed but no Signal K host was stored");

                    return;
                }


                ESP_LOGI(
                    TAG,
                    "Starting WS after discovery: %s:%d",
                    host.c_str(),
                    port);


                signalk_ws_begin(
                    host.c_str(),
                    port);


                websocket_started =
                    true;


                ESP_LOGI(
                    TAG,
                    "Signal K WebSocket started");
            }


            // -----------------------------------------------------------------
            // Fetch vessel information once, AFTER discovery.
            //
            // getVesselInfo() itself runs in another task because it performs
            // several HTTP requests.
            // -----------------------------------------------------------------

            if (websocket_started &&
                !vessel_info_started) {

                vessel_info_started =
                    true;


                BaseType_t result =
                    xTaskCreate(
                        vesselInfoTask,
                        "sk_vessel_info",
                        8192,
                        nullptr,
                        1,
                        nullptr);


                if (result != pdPASS) {

                    ESP_LOGE(
                        TAG,
                        "FAILED to create vessel information task");

                    vessel_info_started =
                        false;
                }
            }
        });
}