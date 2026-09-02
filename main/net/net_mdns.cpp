#include "net_mdns.h"
#include "net_signalk_http.h"

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
        ESP_LOGD(TAG, "DNS lookup failed for %s (err=%d)", hostname, err);
        return false;
    }

    if (addr.type != IPADDR_TYPE_V4) {
        ESP_LOGD(TAG, "Resolved non-IPv4 address for %s", hostname);
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
        ESP_LOGD(TAG, "No results for %s.%s", service, proto);
        return false;
    }

    for (mdns_result_t *r = results; r; r = r->next) {
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

    ESP_LOGD(TAG, "No usable IPv4 found for %s.%s", service, proto);
    mdns_query_results_free(results);
    return false;
}

bool discover_n_config(void)
{
    static bool mdns_started = false;

    Preferences prefs;
    prefs.begin("signalk", true);
    String host = prefs.getString(SK_MANUAL_HOST_PREF, "");
    if (host.length() == 0) {
        host = prefs.getString("signalk_manual_host", "");
    }
    if (host.length() > 0) {
        int manualPort = prefs.getInt(SK_MANUAL_PORT_PREF, 3000);
        if (manualPort == 3000 && prefs.isKey("signalk_manual_port")) {
            manualPort = prefs.getInt("signalk_manual_port", 3000);
        }
        prefs.end();

        preferences.begin("signalk", false);
        preferences.putString(SK_TCP_HOST_PREF, host.c_str());
        preferences.putInt(SK_TCP_PORT_PREF, manualPort);
        preferences.putString(SK_HTTP_HOST_PREF, host.c_str());
        preferences.putInt(SK_HTTP_PORT_PREF, manualPort);
        preferences.end();

        ESP_LOGI(TAG, "SignalK using manual override: %s:%d", host.c_str(), manualPort);
        return true;
    }
    prefs.end();

    if (!mdns_started) {
        ESP_ERROR_CHECK(mdns_init());
        ESP_LOGI(TAG, "mDNS initialized (hostname mode only)");

        mdns_hostname_set("esp32-browser");
        mdns_instance_name_set("esp32-device");

        mdns_started = true;
    }

    bool saved = false;
    std::string ip;
    int port = 3000;

    const char* service_names[] = {"_signalk-ws", "_signalk-http"};
    for (const char* service : service_names) {
        if (mdns_lookup(service, "_tcp", ip, port)) {
            preferences.begin("signalk", false);
            preferences.putString(SK_TCP_HOST_PREF, ip.c_str());
            preferences.putInt(SK_TCP_PORT_PREF, port);
            preferences.putString("sk_http_host", ip.c_str());
            preferences.putInt("sk_http_port", port);
            preferences.end();

            saved = true;
            ESP_LOGI(TAG, "SignalK discovered via mDNS: %s:%d (%s)", ip.c_str(), port, service);
            return true;
        }
    }

    if (resolve_hostname("signalk.local", ip, port) || resolve_hostname("signalk", ip, port)) {
        preferences.begin("signalk", false);
        preferences.putString(SK_TCP_HOST_PREF, ip.c_str());
        preferences.putInt(SK_TCP_PORT_PREF, port);
        preferences.putString("sk_http_host", ip.c_str());
        preferences.putInt("sk_http_port", port);
        preferences.end();

        saved = true;
        ESP_LOGI(TAG, "SignalK discovered via hostname: %s:%d", ip.c_str(), port);
    }

    return saved;
}

void erase_mdns_lookups(void)
{
    preferences.begin("signalk", false);
    preferences.remove("signalk_host");
    preferences.remove("signalk_port");
    preferences.remove(SK_MANUAL_HOST_PREF);
    preferences.remove(SK_MANUAL_PORT_PREF);
    preferences.remove("signalk_manual_host");
    preferences.remove("signalk_manual_port");
    preferences.remove(SK_HTTP_HOST_PREF);
    preferences.remove(SK_HTTP_PORT_PREF);
    preferences.end();
}