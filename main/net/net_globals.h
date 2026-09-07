#ifndef NET_GLOBALS_H
#define NET_GLOBALS_H

#include <ReactESP.h>
#include <MQTTClient.h>
#include <WiFiClient.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern reactesp::ReactESP app;

extern WiFiClient mqttNetClient;
extern MQTTClient mqttClient;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Spawns the background FreeRTOS task on Core 0 to handle
 *        all incoming Signal K WebSocket data loops.
 */
void start_network_processing();

#ifdef __cplusplus
}
#endif

#endif