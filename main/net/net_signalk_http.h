#ifndef NET_SIGNALK_HTTP_H
#define NET_SIGNALK_HTTP_H

#ifdef __cplusplus
extern "C" {
#endif

static const char* PROGMEM SK_TCP_HOST_PREF = "signalk_host";
static const char* PROGMEM SK_TCP_PORT_PREF = "signalk_port";

static const char* PROGMEM SK_HTTP_HOST_PREF = "sk_http_host";
static const char* PROGMEM SK_HTTP_PORT_PREF = "sk_http_port";

static const char* PROGMEM PYP_TCP_HOST_PREF = "pypilot_host";
static const char* PROGMEM PYP_TCP_PORT_PREF = "pypilot_port";

static const char* PROGMEM NMEA0183_TCP_HOST_PREF = "n0183_host";
static const char* PROGMEM NMEA0183_TCP_PORT_PREF = "n0183_port";

static const char* PROGMEM MPD_TCP_HOST_PREF = "mpd_host";
static const char* PROGMEM MPD_TCP_PORT_PREF = "mpd_port";

static const char* PROGMEM VENUS_MQTT_HOST_PREF = "ve_mqtt_host";
static const char* PROGMEM VENUS_MQTT_PORT_PREF = "ve_mqtt_port";

static bool mdns_up = false;

#include <Arduino.h>
#include <Stream.h>

void set_vessel_nav_state(String& val);
bool signalk_parse(Stream& stream);
void getVesselInfo();

#ifdef __cplusplus
}
#endif

#endif