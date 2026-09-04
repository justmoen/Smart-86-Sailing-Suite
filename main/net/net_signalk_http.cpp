#include <StreamString.h>
#include <ui_settings_wifi.h>
#include <ship_data_model.h>
#include "net_signalk_http.h"
#include "net_http.h"
#include "signalk_path_config.h"
#include <Preferences.h>

extern ship_data_t shipDataModel;

static String signalk_http_host_cached = "";
static int signalk_http_port_cached = 0;
static bool http_config_loaded = false;

const char* PROGMEM SK_HTTP_HOST_PREF = "sk_http_host";
const char* PROGMEM SK_HTTP_PORT_PREF = "sk_http_port";

static void load_http_config() {
  if (!http_config_loaded) {
    Preferences prefs;
    prefs.begin("signalk", true);

    if (prefs.isKey(SK_HTTP_HOST_PREF)) {
      signalk_http_host_cached = prefs.getString(SK_HTTP_HOST_PREF, "signalk.local");
    } else if (prefs.isKey("signalk_host")) {
      signalk_http_host_cached = prefs.getString("signalk_host", "signalk.local");
    } else {
      signalk_http_host_cached = "signalk.local";
    }

    if (prefs.isKey(SK_HTTP_PORT_PREF)) {
      signalk_http_port_cached = prefs.getInt(SK_HTTP_PORT_PREF, 3000);
    } else if (prefs.isKey("signalk_port")) {
      signalk_http_port_cached = prefs.getInt("signalk_port", 3000);
    } else {
      signalk_http_port_cached = 3000;
    }
    prefs.end();
    http_config_loaded = true;
  }
}

String signalk_http_host_getter() {
  load_http_config();
  return signalk_http_host_cached;
}

int signalk_http_port_getter() {
  load_http_config();
  return signalk_http_port_cached;
}

#ifdef __cplusplus
extern "C" {
#endif

  void getVesselInfo() {
    String host = signalk_http_host_getter();
    int port = signalk_http_port_getter();
    if (host.length() > 0 && port > 0) {
      const auto& config = get_signalk_path_config();
      String api = String("http://") += host += String(":") += String(port) += "/signalk/v1/api/";
      String resp = httpGETRequest((api + config.vessel_design_beam_api).c_str());
      if (resp.length() > 0) {
        shipDataModel.design.beam.m = resp.toFloat();
        shipDataModel.design.beam.age = millis();
      }
      resp = httpGETRequest((api + config.vessel_design_air_height_api).c_str());
      if (resp.length() > 0) {
        shipDataModel.design.air_height.m = resp.toFloat();
        shipDataModel.design.air_height.age = millis();
      }
      resp = httpGETRequest((api + config.vessel_design_draft_api).c_str());
      if (resp.length() > 0) {
        shipDataModel.design.draft.m = resp.toFloat();
        shipDataModel.design.draft.age = millis();
      }
      resp = httpGETRequest((api + config.vessel_design_length_api).c_str());
      if (resp.length() > 0) {
        shipDataModel.design.length.m = resp.toFloat();
        shipDataModel.design.length.age = millis();
      }
      resp = httpGETRequest((api + config.vessel_name_api).c_str());
      if (resp.length() > 0 && resp.indexOf("<") < 0
          && resp.indexOf("=") < 0 && resp.indexOf(":") < 0) {
        resp.replace("\"", "");
        strncpy(shipDataModel.vessel.name, resp.c_str(), sizeof(shipDataModel.vessel.name) - 1);
      }
      resp = httpGETRequest((api + config.vessel_mmsi_api).c_str());
      if (resp.length() > 0 && resp.indexOf("<") < 0
          && resp.indexOf("=") < 0 && resp.indexOf(":") < 0) {
        resp.replace("\"", "");
        snprintf(shipDataModel.vessel.mmsi, sizeof(shipDataModel.vessel.mmsi), "%s", resp.c_str());
      }
      resp = httpGETRequest((api + config.vessel_navigation_state_api).c_str());
      if (resp.length() > 0) {
        resp.replace("\"", "");
        set_vessel_nav_state(resp);
      }
    }
  }

#ifdef __cplusplus
} /*extern "C"*/
#endif
