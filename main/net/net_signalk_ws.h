#ifndef NET_SIGNALK_WS_H
#define NET_SIGNALK_WS_H

#include "net_types.h"
#include "net_mdns.h"
#include "ui_settings_wifi.h"
#include <WebSocketsClient.h>
#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

extern String signalk_ws_host;
extern int signalk_ws_port;
extern WebSocketsClient webSocket;

void signalk_ws_begin(const char* host, int port);
void signalk_ws_loop();

#ifdef __cplusplus
}
#endif

#endif