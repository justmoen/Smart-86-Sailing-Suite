#ifndef NET_SIGNALK_TCP_H
#define NET_SIGNALK_TCP_H

#include "net_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <Arduino.h>    

void setup_signalk_reconnect(NetClient& client, const char* host, int port);
void signalk_greet(WiFiClient& client);
void set_vessel_nav_state(String& val);
bool signalk_parse(Stream& stream);

#ifdef __cplusplus
}
#endif

#endif