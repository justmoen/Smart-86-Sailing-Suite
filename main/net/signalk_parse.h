#ifndef SIGNALK_PARSE_H
#define SIGNALK_PARSE_H

#include <Arduino.h>
#include <Stream.h>

#ifdef __cplusplus
extern "C" {
#endif

void set_vessel_nav_state(String& val);
bool signalk_parse(Stream& stream);

#ifdef __cplusplus
}
#endif

#endif