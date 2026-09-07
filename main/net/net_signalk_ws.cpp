#include "net_signalk_ws.h"
#include <WiFi.h>
#define WEBSOCKETS_NETWORK_TYPE NETWORK_ESP32
#define WEBSOCKETS_USE_SSL 0
#include <WebSocketsClient.h>
#include "signalk_parse.h"
#include <StreamString.h>

String signalk_ws_host;
int signalk_ws_port;
static WebSocketsClient webSocket;

static void webSocketEvent(WStype_t type, uint8_t * payload, size_t length)
{
    switch(type) {

        case WStype_DISCONNECTED:
            // The payload often contains the text reason or status code
            if (payload != nullptr && length > 0) {
                ESP_LOGW("WS_EVENT", "Disconnected! Server Reason: %s", (char*)payload);
            } else {
                ESP_LOGW("WS_EVENT", "Disconnected with no clear payload reason.");
            }
            break;

        case WStype_CONNECTED:
            ESP_LOGI("WS", "Connected to SignalK");
            webSocket.sendTXT("{\"context\":\"*\",\"subscribe\":[{\"path\":\"*\"}]}");
            break;

        case WStype_TEXT:
        {
            StreamString stream;
            for (size_t i = 0; i < length; i++) {
                stream.write(payload[i]);
            }

            String msg = String((char*)payload);
            stream.print(msg);

            signalk_parse((const char*)payload, length);
            break;
        }

        case WStype_PING:
            ESP_LOGD("WS_EVENT", "Received Ping from Server");
            break;
            
        case WStype_PONG:
            ESP_LOGD("WS_EVENT", "Sent Pong to Server");
            break;

        case WStype_ERROR:
            ESP_LOGE("WS", "WebSocket error");
            break;

        default:
            break;
    }
}

void signalk_ws_begin(const char* host, int port)
{
    // Fix the syntax format error (? instead of &) to avoid server rejection
    webSocket.begin(host, port, "/signalk/v1/stream?subscribe=self&period=1000");
    // Add custom connection headers so Signal K processes the request instantly
    webSocket.setExtraHeaders("User-Agent: Smart-86-Sailing-Suite\r\nOrigin: http://192.168.1.71:8080");
    webSocket.onEvent(webSocketEvent);

    // REMOVE explicit active heartbeat tracking to prevent the 6-second timeout crash.
    // The library automatically responds to incoming server Pings with Pongs 
    // natively on the back-end as long as its loop is running smoothly.

    // Lower the retry wait interval so it picks up quickly if the network drops
    webSocket.setReconnectInterval(5000); 
}


void signalk_ws_loop()
{
    webSocket.loop();
}

bool signalk_ws_is_connected() {
    return webSocket.isConnected();
}

void signalk_ws_send(const char* msg) {
    webSocket.sendTXT(msg);
}