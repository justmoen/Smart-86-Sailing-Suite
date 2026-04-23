#ifndef NET_SIGNALK_WS_H
#define NET_SIGNALK_WS_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

extern String signalk_ws_host;
extern int signalk_ws_port;

void signalk_ws_begin(const char* host, int port);
void signalk_ws_loop();
bool signalk_ws_is_connected();
void signalk_ws_send(const char* msg);

#ifdef __cplusplus
}
#endif

#endif