#include "net_mdns.h"

#include "mdns.h"
#include "esp_log.h"
#include <string>
#include <Preferences.h>
#include <lwip/dns.h>
#include <lwip/ip_addr.h>

Preferences preferences;

static const char* TAG = "MDNS";

/* -------------------------------------------------- */
/* Helper: resolve hostname                           */
/* -------------------------------------------------- */

static bool resolve_hostname(const char* hostname,
                             std::string &ip_out, int &port_out)
{
    ip_addr_t addr;

    esp_err_t err = dns_gethostbyname(hostname, &addr, NULL, NULL);
    if (err != ERR_OK) {
        ESP_LOGW(TAG, "DNS lookup failed for %s (err=%d)", hostname, err);
        return false;
    }

    if (addr.type != IPADDR_TYPE_V4) {
        ESP_LOGW(TAG, "Resolved non-IPv4 address for %s", hostname);
        return false;
    }

    char ip_str[16];
    ip4addr_ntoa_r(ip_2_ip4(&addr), ip_str, sizeof(ip_str));

    ip_out = ip_str;

    // 👉 SignalK default TCP port
    port_out = 3000;

    ESP_LOGI(TAG, "Resolved %s → %s:%d", hostname, ip_str, port_out);

    return true;
}

/* -------------------------------------------------- */
/* Public                                             */
/* -------------------------------------------------- */
static bool mdns_lookup(const char* service, const char* proto,
                        std::string &ip_out, int &port_out)
{
    mdns_result_t *results = NULL;

    ESP_LOGI(TAG, "Querying mDNS: %s.%s", service, proto);

    esp_err_t err = mdns_query_ptr(service, proto, 3000, 20, &results);
    if (err != ESP_OK || !results) {
        ESP_LOGW(TAG, "No results for %s.%s", service, proto);
        return false;
    }

    // 🔍 DEBUG: print everything we get
    for (mdns_result_t *r = results; r; r = r->next) {
        ESP_LOGI(TAG, "---- RESULT ----");
        ESP_LOGI(TAG, "Instance: %s", r->instance_name ? r->instance_name : "NULL");
        ESP_LOGI(TAG, "Hostname: %s", r->hostname ? r->hostname : "NULL");
        ESP_LOGI(TAG, "Port: %d", r->port);

        for (mdns_ip_addr_t *a = r->addr; a; a = a->next) {
            if (a->addr.type == ESP_IPADDR_TYPE_V4) {
                char ip_str[16];
                snprintf(ip_str, sizeof(ip_str), IPSTR,
                         IP2STR(&a->addr.u_addr.ip4));

                ESP_LOGI(TAG, "IPv4: %s", ip_str);

                // ✅ USE FIRST VALID IPv4
                ip_out = ip_str;
                port_out = r->port;

                mdns_query_results_free(results);
                return true;
            }
        }
    }

    ESP_LOGW(TAG, "No usable IPv4 found");
    mdns_query_results_free(results);
    return false;
}

bool discover_n_config(void)
{
    static bool mdns_started = false;

    if (!mdns_started) {
        ESP_ERROR_CHECK(mdns_init());
        ESP_LOGI(TAG, "mDNS initialized (hostname mode only)");

        mdns_hostname_set("esp32-browser");
        mdns_instance_name_set("esp32-device");

        mdns_started = true;
    }

    bool saved = false;

    std::string ip;
    int port;

    if (mdns_lookup("_signalk-ws", "_tcp", ip, port)) {
        preferences.putString(SK_TCP_HOST_PREF, ip.c_str());
        preferences.putInt(SK_TCP_PORT_PREF, port);

        saved = true;

        ESP_LOGI(TAG, "SignalK WS: %s:%d", ip.c_str(), port);
    }// else {
       // ESP_LOGW(TAG, "Failed to resolve %s", hostname);
    //}

    return saved;
}

void erase_mdns_lookups(void)
{
    preferences.remove("signalk_host");
    preferences.remove("signalk_port");
}