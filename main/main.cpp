#include "app/system_init.h"
#include "app/app_startup.h"

// wifi setup
#include <ui_settings_wifi.h>
#include "keepalive.h"
#include "net_mdns.h"

NetClient nmea0183Client;
NetClient skClient;
NetClient pypClient;

reactesp::ReactESP app;

#include "ship_data_model.h"
ship_data_t shipDataModel;
#include "ship_data_util.h"
#include "derived_data.h"

#include <WMM_Tinier.h>
WMM_Tinier myDeclination;

extern "C" void app_main()
{
    init_system();

    settingUpWiFi(start_application);
}
