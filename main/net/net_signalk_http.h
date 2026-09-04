#include "net_mdns.h"
#include <Arduino.h>
#include <Stream.h>
#ifndef NET_SIGNALK_HTTP_H
#define NET_SIGNALK_HTTP_H

#ifdef __cplusplus
extern "C" {
#endif

extern String signalk_http_host;
extern int signalk_http_port;

extern const char* PROGMEM SK_HTTP_HOST_PREF;
extern const char* PROGMEM SK_HTTP_PORT_PREF;

void set_vessel_nav_state(String& val);
void getVesselInfo();

#ifdef __cplusplus
}
#endif

#endif