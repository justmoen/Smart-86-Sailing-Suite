#include "net_signalk_ws.h"

#include <WiFi.h>

#define WEBSOCKETS_NETWORK_TYPE NETWORK_ESP32
#define WEBSOCKETS_USE_SSL 0

#include <WebSocketsClient.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_log.h>

#include "signalk_parse.h"


// =============================================================================
// CONFIGURATION
// =============================================================================

static constexpr const char *TAG = "WS";

// Log connection health periodically.
// This is diagnostic only and can be increased later.
static constexpr uint32_t WS_HEALTH_LOG_INTERVAL_MS = 30000;

// If we haven't received any Signal K data for this long, report it.
// We do NOT automatically disconnect based on this value.
static constexpr uint32_t WS_RX_WARNING_INTERVAL_MS = 60000;


// =============================================================================
// GLOBALS
// =============================================================================

String signalk_ws_host;
int signalk_ws_port = 3000;

static WebSocketsClient webSocket;


// =============================================================================
// CONNECTION STATE
// =============================================================================

static volatile bool ws_connected = false;

static uint32_t ws_connected_at_ms = 0;
static uint32_t ws_last_rx_ms = 0;
static uint32_t ws_last_tx_ms = 0;
static uint32_t ws_last_loop_ms = 0;
static uint32_t ws_last_health_log_ms = 0;
static uint32_t ws_last_ping_ms = 0;

static uint32_t ws_rx_messages = 0;
static uint32_t ws_rx_bytes = 0;

static uint32_t ws_tx_messages = 0;
static uint32_t ws_tx_bytes = 0;

static uint32_t ws_disconnect_count = 0;
static uint32_t ws_error_count = 0;

static uint32_t ws_loop_calls = 0;


// =============================================================================
// HELPERS
// =============================================================================

static uint32_t elapsed_since(uint32_t timestamp)
{
    if (timestamp == 0) {
        return 0;
    }

    return millis() - timestamp;
}


// -----------------------------------------------------------------------------
// Print useful runtime information whenever something happens to the socket.
// -----------------------------------------------------------------------------

static void log_ws_runtime(const char *context)
{
    wl_status_t wifi_status = WiFi.status();

    int rssi = -127;

    if (wifi_status == WL_CONNECTED) {
        rssi = WiFi.RSSI();
    }

    uint32_t free_heap =
        ESP.getFreeHeap();

    uint32_t largest_internal =
        heap_caps_get_largest_free_block(
            MALLOC_CAP_INTERNAL);

    uint32_t free_internal =
        heap_caps_get_free_size(
            MALLOC_CAP_INTERNAL);

    uint32_t free_psram =
        heap_caps_get_free_size(
            MALLOC_CAP_SPIRAM);


    ESP_LOGI(
        TAG,
        "WS STATUS [%s]",
        context);

    ESP_LOGI(
        TAG,
        "  connected=%s",
        ws_connected ? "YES" : "NO");

    ESP_LOGI(
        TAG,
        "  wifi_status=%d",
        wifi_status);

    ESP_LOGI(
        TAG,
        "  wifi_rssi=%d dBm",
        rssi);

    ESP_LOGI(
        TAG,
        "  free_heap=%lu",
        (unsigned long)free_heap);

    ESP_LOGI(
        TAG,
        "  free_internal=%lu",
        (unsigned long)free_internal);

    ESP_LOGI(
        TAG,
        "  largest_internal=%lu",
        (unsigned long)largest_internal);

    ESP_LOGI(
        TAG,
        "  free_psram=%lu",
        (unsigned long)free_psram);

    ESP_LOGI(
        TAG,
        "  rx_messages=%lu",
        (unsigned long)ws_rx_messages);

    ESP_LOGI(
        TAG,
        "  rx_bytes=%lu",
        (unsigned long)ws_rx_bytes);

    ESP_LOGI(
        TAG,
        "  tx_messages=%lu",
        (unsigned long)ws_tx_messages);

    ESP_LOGI(
        TAG,
        "  tx_bytes=%lu",
        (unsigned long)ws_tx_bytes);

    ESP_LOGI(
        TAG,
        "  disconnects=%lu",
        (unsigned long)ws_disconnect_count);

    ESP_LOGI(
        TAG,
        "  errors=%lu",
        (unsigned long)ws_error_count);

    ESP_LOGI(
        TAG,
        "  loop_calls=%lu",
        (unsigned long)ws_loop_calls);

    if (ws_connected) {

        ESP_LOGI(
            TAG,
            "  connected_for=%lu ms",
            (unsigned long)
                elapsed_since(ws_connected_at_ms));

        ESP_LOGI(
            TAG,
            "  last_rx_age=%lu ms",
            (unsigned long)
                elapsed_since(ws_last_rx_ms));

        ESP_LOGI(
            TAG,
            "  last_tx_age=%lu ms",
            (unsigned long)
                elapsed_since(ws_last_tx_ms));

        ESP_LOGI(
            TAG,
            "  last_loop_age=%lu ms",
            (unsigned long)
                elapsed_since(ws_last_loop_ms));
    }
}


// =============================================================================
// WEBSOCKET EVENT HANDLER
// =============================================================================

static void webSocketEvent(
    WStype_t type,
    uint8_t *payload,
    size_t length)
{
    uint32_t now =
        millis();


    switch (type) {

        // ---------------------------------------------------------------------
        // DISCONNECTED
        // ---------------------------------------------------------------------

        case WStype_DISCONNECTED:
        {
            ws_connected = false;

            ws_disconnect_count++;

            uint32_t connected_for =
                ws_connected_at_ms != 0
                    ? now - ws_connected_at_ms
                    : 0;


            ESP_LOGW(
                TAG,
                "==================================================");

            ESP_LOGW(
                TAG,
                "WEBSOCKET DISCONNECTED");

            ESP_LOGW(
                TAG,
                "  disconnect_count=%lu",
                (unsigned long)ws_disconnect_count);

            ESP_LOGW(
                TAG,
                "  connected_for=%lu ms",
                (unsigned long)connected_for);

            ESP_LOGW(
                TAG,
                "  wifi_status=%d",
                WiFi.status());


            if (WiFi.status() == WL_CONNECTED) {

                ESP_LOGW(
                    TAG,
                    "  WiFi is STILL CONNECTED");

                ESP_LOGW(
                    TAG,
                    "  WiFi RSSI=%d dBm",
                    WiFi.RSSI());

            } else {

                ESP_LOGW(
                    TAG,
                    "  WiFi is NOT connected");
            }


            ESP_LOGW(
                TAG,
                "  rx_messages=%lu",
                (unsigned long)ws_rx_messages);

            ESP_LOGW(
                TAG,
                "  rx_bytes=%lu",
                (unsigned long)ws_rx_bytes);

            ESP_LOGW(
                TAG,
                "  tx_messages=%lu",
                (unsigned long)ws_tx_messages);

            ESP_LOGW(
                TAG,
                "  tx_bytes=%lu",
                (unsigned long)ws_tx_bytes);

            ESP_LOGW(
                TAG,
                "  last_rx_age=%lu ms",
                (unsigned long)
                    elapsed_since(ws_last_rx_ms));

            ESP_LOGW(
                TAG,
                "  last_tx_age=%lu ms",
                (unsigned long)
                    elapsed_since(ws_last_tx_ms));

            ESP_LOGW(
                TAG,
                "  last_loop_age=%lu ms",
                (unsigned long)
                    elapsed_since(ws_last_loop_ms));


            // -----------------------------------------------------------------
            // IMPORTANT:
            //
            // Do NOT interpret payload as a WebSocket close reason here.
            //
            // The WebSocketsClient library does not provide a reliable
            // close-code/reason payload through WStype_DISCONNECTED.
            // -----------------------------------------------------------------

            if (payload != nullptr && length > 0) {

                ESP_LOGW(
                    TAG,
                    "  disconnect payload length=%u",
                    (unsigned)length);

            } else {

                ESP_LOGW(
                    TAG,
                    "  no disconnect payload supplied by library");
            }


            log_ws_runtime(
                "DISCONNECTED");

            ESP_LOGW(
                TAG,
                "==================================================");

            break;
        }


        // ---------------------------------------------------------------------
        // CONNECTED
        // ---------------------------------------------------------------------

        case WStype_CONNECTED:
        {
            ws_connected = true;

            ws_connected_at_ms = now;

            ws_last_rx_ms = now;
            ws_last_tx_ms = now;
            ws_last_ping_ms = now;

            ws_rx_messages = 0;
            ws_rx_bytes = 0;

            ws_tx_messages = 0;
            ws_tx_bytes = 0;


            ESP_LOGI(
                TAG,
                "==================================================");

            ESP_LOGI(
                TAG,
                "CONNECTED TO SIGNAL K");

            ESP_LOGI(
                TAG,
                "  host=%s",
                signalk_ws_host.c_str());

            ESP_LOGI(
                TAG,
                "  port=%d",
                signalk_ws_port);

            ESP_LOGI(
                TAG,
                "  wifi_status=%d",
                WiFi.status());

            if (WiFi.status() == WL_CONNECTED) {

                ESP_LOGI(
                    TAG,
                    "  wifi_rssi=%d dBm",
                    WiFi.RSSI());
            }


            // -----------------------------------------------------------------
            // IMPORTANT:
            //
            // We intentionally DO NOT send another subscription here.
            //
            // The URL already contains:
            //
            //   ?subscribe=self
            //
            // Sending another {"subscribe":...} message immediately after
            // connecting is unnecessary and can make diagnosis harder.
            // -----------------------------------------------------------------

            ESP_LOGI(
                TAG,
                "Using subscription specified by WebSocket URL");

            ESP_LOGI(
                TAG,
                "==================================================");

            break;
        }


        // ---------------------------------------------------------------------
        // TEXT DATA
        // ---------------------------------------------------------------------

        case WStype_TEXT:
        {
            ws_last_rx_ms = now;

            ws_rx_messages++;

            ws_rx_bytes +=
                (uint32_t)length;


            // -----------------------------------------------------------------
            // Do not create StreamString or another String copy here.
            //
            // signalk_parse() already accepts the payload directly.
            // -----------------------------------------------------------------

            signalk_parse(
                (const char *)payload,
                length);


            break;
        }


        // ---------------------------------------------------------------------
        // BINARY
        // ---------------------------------------------------------------------

        case WStype_BIN:
        {
            ESP_LOGD(
                TAG,
                "Received binary WebSocket message: %u bytes",
                (unsigned)length);

            break;
        }


        // ---------------------------------------------------------------------
        // PING
        // ---------------------------------------------------------------------

        case WStype_PING:
        {
            ESP_LOGD(
                TAG,
                "Received WebSocket PING");

            // The library normally handles the corresponding PONG.
            break;
        }


        // ---------------------------------------------------------------------
        // PONG
        // ---------------------------------------------------------------------

        case WStype_PONG:
        {
            ESP_LOGD(
                TAG,
                "Received WebSocket PONG");

            break;
        }


        // ---------------------------------------------------------------------
        // ERROR
        // ---------------------------------------------------------------------

        case WStype_ERROR:
        {
            ws_error_count++;

            ESP_LOGE(
                TAG,
                "==================================================");

            ESP_LOGE(
                TAG,
                "WEBSOCKET ERROR");

            ESP_LOGE(
                TAG,
                "  error_count=%lu",
                (unsigned long)ws_error_count);

            ESP_LOGE(
                TAG,
                "  payload_length=%u",
                (unsigned)length);

            if (payload != nullptr &&
                length > 0) {

                // Make a bounded printable copy.
                char error_text[160];

                size_t copy_length =
                    length;

                if (copy_length >=
                    sizeof(error_text)) {

                    copy_length =
                        sizeof(error_text) - 1;
                }

                memcpy(
                    error_text,
                    payload,
                    copy_length);

                error_text[copy_length] =
                    '\0';

                ESP_LOGE(
                    TAG,
                    "  payload='%s'",
                    error_text);
            }

            log_ws_runtime(
                "ERROR");

            ESP_LOGE(
                TAG,
                "==================================================");

            break;
        }


        default:
            break;
    }
}


// =============================================================================
// BEGIN
// =============================================================================

void signalk_ws_begin(
    const char *host,
    int port)
{
    signalk_ws_host =
        host != nullptr
            ? host
            : "";

    signalk_ws_port =
        port;


    ESP_LOGI(
        TAG,
        "Starting Signal K WebSocket");

    ESP_LOGI(
        TAG,
        "  host=%s",
        signalk_ws_host.c_str());

    ESP_LOGI(
        TAG,
        "  port=%d",
        signalk_ws_port);


    // -------------------------------------------------------------------------
    // Signal K WebSocket endpoint.
    //
    // Keep the initial subscription simple.
    //
    // "self" avoids receiving the entire Signal K data tree for every vessel.
    // -------------------------------------------------------------------------

    String url =
        "/signalk/v1/stream?subscribe=self";


    ESP_LOGI(
        TAG,
        "  path=%s",
        url.c_str());


    webSocket.begin(
        signalk_ws_host.c_str(),
        signalk_ws_port,
        url.c_str());


    webSocket.onEvent(
        webSocketEvent);


    // -------------------------------------------------------------------------
    // Reconnect
    // -------------------------------------------------------------------------

    webSocket.setReconnectInterval(
        5000);


    ESP_LOGI(
        TAG,
        "Signal K WebSocket client configured");
}


// =============================================================================
// LOOP
// =============================================================================

void signalk_ws_loop()
{
    ws_last_loop_ms =
        millis();

    ws_loop_calls++;


    // -------------------------------------------------------------------------
    // The WebSocketsClient library MUST be serviced continuously.
    //
    // This is responsible for:
    //
    //   - receiving frames
    //   - sending/processing PONG
    //   - processing TCP state
    //   - processing reconnects
    //   - detecting disconnects
    //
    // If this function is not called frequently, the WebSocket can appear to
    // randomly disconnect.
    // -------------------------------------------------------------------------

    webSocket.loop();


    uint32_t now =
        millis();


    // -------------------------------------------------------------------------
    // Periodic diagnostic output.
    // -------------------------------------------------------------------------

    if (ws_connected &&
        now - ws_last_health_log_ms >=
            WS_HEALTH_LOG_INTERVAL_MS) {

        ws_last_health_log_ms =
            now;


        ESP_LOGI(
            TAG,
            "WS health: connected_for=%lu s rx=%lu msgs/%lu bytes "
            "last_rx=%lu ms last_loop=%lu ms",
            (unsigned long)
                ((now - ws_connected_at_ms) / 1000),
            (unsigned long)
                ws_rx_messages,
            (unsigned long)
                ws_rx_bytes,
            (unsigned long)
                elapsed_since(ws_last_rx_ms),
            (unsigned long)
                elapsed_since(ws_last_loop_ms));


        // ---------------------------------------------------------------------
        // Warn if Signal K data has stopped arriving.
        //
        // This is diagnostic only. Do NOT disconnect automatically.
        // ---------------------------------------------------------------------

        if (elapsed_since(ws_last_rx_ms) >
            WS_RX_WARNING_INTERVAL_MS) {

            ESP_LOGW(
                TAG,
                "No Signal K WebSocket data received for %lu ms",
                (unsigned long)
                    elapsed_since(ws_last_rx_ms));
        }
    }
}


// =============================================================================
// STATUS
// =============================================================================

bool signalk_ws_is_connected()
{
    return ws_connected &&
           webSocket.isConnected();
}
