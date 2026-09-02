// keepalive.h
#ifndef KEEPALIVE_H
#define KEEPALIVE_H

#include "net_types.h"
#include "net_globals.h"
#include <WiFiClient.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const char *BLANK_IP;

void setKeepAlive(WiFiClient &wclient);
void disconnect_clients();
void ESP_restart();

#ifdef __cplusplus
}
#endif
#endif