#ifndef NET_MDNS_H
#define NET_MDNS_H

#include <Preferences.h>

#ifdef __cplusplus
extern "C" {
#endif

extern Preferences preferences;

extern const char* SK_TCP_HOST_PREF;
extern const char* SK_TCP_PORT_PREF;
extern const char* SK_MANUAL_HOST_PREF;
extern const char* SK_MANUAL_PORT_PREF;

bool discover_n_config();
void erase_mdns_lookups();

#ifdef __cplusplus
}
#endif

#endif