#ifndef NET_GLOBALS_H
#define NET_GLOBALS_H

#include <ReactESP.h>
#include <MQTT.h>

extern reactesp::ReactESP app;

extern WiFiClient mqttNetClient;
extern MQTTClient mqttClient;

#endif