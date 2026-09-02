#ifndef NET_TYPES_H
#define NET_TYPES_H

#include <WiFiClient.h>

typedef struct _NetClient {
  WiFiClient c;
  unsigned long lastActivity;
} NetClient;

extern NetClient nmea0183Client;
extern NetClient skClient;
extern NetClient pypClient;

#endif