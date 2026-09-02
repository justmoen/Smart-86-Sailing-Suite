// signalk_connection_manager.cpp
#include <net_globals.h>
#include "net_mdns.h"
#include "net_signalk_ws.h"

void start_signalk_connection_task() {
    app.onRepeat(2000, []() {
        static bool connected = false;

        if (!connected) {
            bool found = discover_n_config();

            if (found) {
                preferences.begin("signalk", true);  // Open in read-only mode
                String host = preferences.getString(SK_TCP_HOST_PREF, "");
                int port = preferences.getInt(SK_TCP_PORT_PREF, 3000);
                preferences.end();

                ESP_LOGI("WS", "Starting WS after discovery: %s:%d",
                        host.c_str(), port);

                signalk_ws_begin(host.c_str(), port);
                connected = true;
            }
        }
    });
}