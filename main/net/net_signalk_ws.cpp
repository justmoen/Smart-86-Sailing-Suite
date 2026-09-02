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

        case WStype_DISCONNECTED:
            ESP_LOGW("WS", "Disconnected");
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
    // Do not force a subprotocol. SignalK accepts generic WebSocket connections
    // without a negotiated protocol; advertising "arduino" can be rejected by the
    // server and results in immediate reset/disconnects.
    webSocket.begin(host, port, "/signalk/v1/stream", "");
    webSocket.onEvent(webSocketEvent);

    // Newer SignalK versions enforce websocket ping/pong at the protocol level.
    // Our client was not responding correctly to those heartbeat frames, which
    // caused the socket to be dropped and immediately reconnected in a loop.
    // We intentionally disable the client-side heartbeat to avoid tripping the
    // server's native timeout while still allowing a clean reconnect path.
    webSocket.setReconnectInterval(15000);
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