#ifndef NET_MDNS_H
#define NET_MDNS_H

#include <Preferences.h>

#ifdef __cplusplus
extern "C" {
#endif

extern Preferences preferences;

/* keys */
static const char* SK_TCP_HOST_PREF = "signalk_host";
static const char* SK_TCP_PORT_PREF = "signalk_port";

bool discover_n_config();
void erase_mdns_lookups();

#ifdef __cplusplus
}
#endif

#endif