#include "net_mdns.h"
#include "net_signalk_http.h"

#include "mdns.h"
#include "esp_log.h"

#include <string>
#include <Preferences.h>
#include <lwip/dns.h>
#include <lwip/ip_addr.h>

Preferences preferences;

static const char *TAG = "MDNS";

/* keys */
const char* SK_TCP_HOST_PREF = "signalk_host";
const char* SK_TCP_PORT_PREF = "signalk_port";
const char* SK_MANUAL_HOST_PREF = "sk_man_host";
const char* SK_MANUAL_PORT_PREF = "sk_man_port";


// -----------------------------------------------------------------------------
// HOSTNAME RESOLUTION
//
// This function may block.
//
// It is ONLY called from the Signal K discovery FreeRTOS task.
// -----------------------------------------------------------------------------

static bool resolve_hostname(
    const char *hostname,
    std::string &ip_out,
    int &port_out)
{
    ESP_LOGI(
        TAG,
        "Attempting hostname resolution: '%s'",
        hostname);

    ip_addr_t addr;

    esp_err_t err =
        dns_gethostbyname(
            hostname,
            &addr,
            NULL,
            NULL);

    if (err != ERR_OK) {
        ESP_LOGW(
            TAG,
            "DNS lookup failed for '%s' (err=%d)",
            hostname,
            err);

        return false;
    }

    if (addr.type != IPADDR_TYPE_V4) {
        ESP_LOGW(
            TAG,
            "Resolved non-IPv4 address for '%s'",
            hostname);

        return false;
    }

    char ip_str[16];

    ip4addr_ntoa_r(
        ip_2_ip4(&addr),
        ip_str,
        sizeof(ip_str));

    ip_out = ip_str;

    // Signal K default HTTP/WebSocket port.
    port_out = 3000;

    ESP_LOGI(
        TAG,
        "Resolved '%s' -> %s:%d",
        hostname,
        ip_str,
        port_out);

    return true;
}


// -----------------------------------------------------------------------------
// mDNS SERVICE LOOKUP
//
// This function may block for the duration of the mDNS query.
//
// It is ONLY called from the Signal K discovery FreeRTOS task.
// -----------------------------------------------------------------------------

static bool mdns_lookup(
    const char *service,
    const char *proto,
    std::string &ip_out,
    int &port_out)
{
    mdns_result_t *results = NULL;

    ESP_LOGI(
        TAG,
        "Querying mDNS: %s.%s",
        service,
        proto);

    esp_err_t err =
        mdns_query_ptr(
            service,
            proto,
            3000,
            20,
            &results);

    if (err != ESP_OK) {
        ESP_LOGW(
            TAG,
            "mDNS query failed for %s.%s (err=%d)",
            service,
            proto,
            err);

        return false;
    }

    if (results == NULL) {
        ESP_LOGI(
            TAG,
            "No mDNS results for %s.%s",
            service,
            proto);

        return false;
    }

    for (
        mdns_result_t *r = results;
        r != NULL;
        r = r->next) {

        for (
            mdns_ip_addr_t *a = r->addr;
            a != NULL;
            a = a->next) {

            if (a->addr.type != ESP_IPADDR_TYPE_V4) {
                continue;
            }

            char ip_str[16];

            snprintf(
                ip_str,
                sizeof(ip_str),
                IPSTR,
                IP2STR(&a->addr.u_addr.ip4));

            ESP_LOGI(
                TAG,
                "mDNS result: %s:%u",
                ip_str,
                r->port);

            ip_out = ip_str;
            port_out = r->port;

            mdns_query_results_free(results);

            return true;
        }
    }

    ESP_LOGI(
        TAG,
        "mDNS returned no usable IPv4 address for %s.%s",
        service,
        proto);

    mdns_query_results_free(results);

    return false;
}


// -----------------------------------------------------------------------------
// STORE DISCOVERED SIGNAL K SERVER
//
// Uses a LOCAL Preferences object.
//
// Do not use the global `preferences` object here. The discovery task is
// independent of the main application and should not depend on the state of
// that global Preferences instance.
// -----------------------------------------------------------------------------

static bool store_signalk_server(
    const char *host,
    int port)
{
    Preferences prefs;

    if (!prefs.begin("signalk", false)) {
        ESP_LOGE(
            TAG,
            "FAILED to open Signal K Preferences for writing");

        return false;
    }

    prefs.putString(
        SK_TCP_HOST_PREF,
        host);

    prefs.putInt(
        SK_TCP_PORT_PREF,
        port);

    prefs.putString(
        SK_HTTP_HOST_PREF,
        host);

    prefs.putInt(
        SK_HTTP_PORT_PREF,
        port);

    prefs.end();

    ESP_LOGI(
        TAG,
        "Stored Signal K server: %s:%d",
        host,
        port);

    return true;
}


// -----------------------------------------------------------------------------
// SIGNAL K DISCOVERY
//
// IMPORTANT:
//
// This function can block.
//
// It MUST be executed from the dedicated Signal K discovery FreeRTOS task.
//
// Never call this function directly from the main ReactESP loop.
// -----------------------------------------------------------------------------

bool discover_n_config(void)
{
    static bool mdns_started = false;

    // -------------------------------------------------------------------------
    // Check for manually configured Signal K host first.
    // -------------------------------------------------------------------------

    Preferences prefs;

    if (!prefs.begin("signalk", true)) {
        ESP_LOGE(
            TAG,
            "FAILED to open Signal K Preferences");

        return false;
    }

    String host = "";

    // Current/manual key.
    if (prefs.isKey(SK_MANUAL_HOST_PREF)) {
        host =
            prefs.getString(
                SK_MANUAL_HOST_PREF,
                "");
    }

    // Legacy key.
    if (host.length() == 0 &&
        prefs.isKey("signalk_manual_host")) {

        host =
            prefs.getString(
                "signalk_manual_host",
                "");
    }

    if (host.length() > 0) {

        int manualPort = 3000;

        if (prefs.isKey(SK_MANUAL_PORT_PREF)) {
            manualPort =
                prefs.getInt(
                    SK_MANUAL_PORT_PREF,
                    3000);
        }

        // Legacy port key.
        if (manualPort == 3000 &&
            prefs.isKey("signalk_manual_port")) {

            manualPort =
                prefs.getInt(
                    "signalk_manual_port",
                    3000);
        }

        prefs.end();

        ESP_LOGI(
            TAG,
            "Signal K using manual override: %s:%d",
            host.c_str(),
            manualPort);

        return store_signalk_server(
            host.c_str(),
            manualPort);
    }

    prefs.end();


    // -------------------------------------------------------------------------
    // Initialize mDNS once.
    // -------------------------------------------------------------------------

    if (!mdns_started) {

        esp_err_t err =
            mdns_init();

        if (err != ESP_OK) {
            ESP_LOGE(
                TAG,
                "mDNS initialization failed: %d",
                err);

            return false;
        }

        ESP_LOGI(
            TAG,
            "mDNS initialized");

        mdns_hostname_set(
            "esp32-browser");

        mdns_instance_name_set(
            "esp32-device");

        mdns_started = true;
    }


    // -------------------------------------------------------------------------
    // Signal K DNS-SD service discovery.
    //
    // Try WebSocket first because that is what the application actually uses.
    // -------------------------------------------------------------------------

    std::string ip;
    int port = 3000;

    const char *service_names[] = {
        "_signalk-ws",
        "_signalk-http"
    };

    for (
        const char *service :
        service_names) {

        if (
            mdns_lookup(
                service,
                "_tcp",
                ip,
                port)) {

            if (
                store_signalk_server(
                    ip.c_str(),
                    port)) {

                ESP_LOGI(
                    TAG,
                    "Signal K discovered via mDNS: %s:%d (%s)",
                    ip.c_str(),
                    port,
                    service);

                return true;
            }

            return false;
        }
    }


    // -------------------------------------------------------------------------
    // Legacy hostname fallback.
    //
    // This may block, but we are already running in the discovery task.
    // Therefore it cannot block the web UI.
    // -------------------------------------------------------------------------

    ESP_LOGI(
        TAG,
        "mDNS service discovery did not find Signal K");

    ESP_LOGI(
        TAG,
        "Trying legacy hostname fallback");


    if (
        resolve_hostname(
            "signalk.local",
            ip,
            port) ||

        resolve_hostname(
            "signalk",
            ip,
            port)) {

        if (
            store_signalk_server(
                ip.c_str(),
                port)) {

            ESP_LOGI(
                TAG,
                "Signal K discovered via hostname: %s:%d",
                ip.c_str(),
                port);

            return true;
        }

        return false;
    }


    ESP_LOGI(
        TAG,
        "Signal K was not discovered");

    return false;
}


// -----------------------------------------------------------------------------
// ERASE SIGNAL K DISCOVERY SETTINGS
// -----------------------------------------------------------------------------

void erase_mdns_lookups(void)
{
    Preferences prefs;

    if (!prefs.begin(
            "signalk",
            false)) {

        ESP_LOGE(
            TAG,
            "FAILED to open Signal K Preferences for erase");

        return;
    }

    prefs.remove(
        "signalk_host");

    prefs.remove(
        "signalk_port");

    prefs.remove(
        SK_MANUAL_HOST_PREF);

    prefs.remove(
        SK_MANUAL_PORT_PREF);

    prefs.remove(
        "signalk_manual_host");

    prefs.remove(
        "signalk_manual_port");

    prefs.remove(
        SK_HTTP_HOST_PREF);

    prefs.remove(
        SK_HTTP_PORT_PREF);

    prefs.end();

    ESP_LOGI(
        TAG,
        "Signal K discovery settings erased");
}