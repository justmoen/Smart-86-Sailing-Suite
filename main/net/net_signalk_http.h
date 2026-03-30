#include "net_mdns.h"
#ifndef NET_SIGNALK_HTTP_H
#define NET_SIGNALK_HTTP_H

#ifdef __cplusplus
extern "C" {
#endif

static const char* PROGMEM SK_HTTP_HOST_PREF = "sk_http_host";
static const char* PROGMEM SK_HTTP_PORT_PREF = "sk_http_port";

extern String signalk_http_host;
extern int signalk_http_port;

#include <Arduino.h>
#include <Stream.h>

void set_vessel_nav_state(String& val);
void getVesselInfo();

#ifdef __cplusplus
}
#endif

#endif