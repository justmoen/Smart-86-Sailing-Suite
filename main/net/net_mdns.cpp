#include "net_mdns.h"

#include "mdns.h"
#include "esp_log.h"
#include <string>
#include "nvs.h"
#include <Preferences.h>

// you already have this somewhere
extern Preferences preferences;

static const char* TAG = "MDNS";

/* keys */
static const char* SK_TCP_HOST_PREF = "signalk_host";
static const char* SK_TCP_PORT_PREF = "signalk_port";

static bool mdns_up = false;

/* -------------------------------------------------- */
/* Helper                                             */
/* -------------------------------------------------- */

static bool mdns_lookup(const char* service, const char* proto,
                        std::string &ip_out, int &port_out)
{
    mdns_result_t *results = NULL;

    esp_err_t err = mdns_query_ptr(service, proto, 3000, 10, &results);
    if (err != ESP_OK || !results) {
        ESP_LOGW(TAG, "No results for %s.%s", service, proto);
        return false;
    }

    mdns_result_t *r = results;

    if (r->addr) {
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), IPSTR,
                 IP2STR(&r->addr->addr.u_addr.ip4));

        ip_out = ip_str;
        port_out = r->port;

        mdns_query_results_free(results);
        return true;
    }

    mdns_query_results_free(results);
    return false;
}

/* -------------------------------------------------- */
/* Public                                             */
/* -------------------------------------------------- */

bool discover_n_config(void)
{
    static bool mdns_started = false;

    if (!mdns_started) {
        mdns_init();
        mdns_hostname_set("esp32-browser");
        mdns_started = true;
    }

    bool saved = false;

    std::string ip;
    int port;

    if (mdns_lookup("signalk-tcp", "tcp", ip, port)) {
        preferences.putString(SK_TCP_HOST_PREF, ip.c_str());
        preferences.putInt(SK_TCP_PORT_PREF, port);
        saved = true;

        ESP_LOGI(TAG, "SignalK TCP: %s:%d", ip.c_str(), port);
    }

    return saved;
}

void erase_mdns_lookups(void)
{
    preferences.remove("signalk_host");
    preferences.remove("signalk_port");
}