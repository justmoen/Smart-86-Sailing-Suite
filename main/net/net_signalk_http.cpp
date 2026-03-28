#include <StreamString.h>
#include <ui_settings_wifi.h>
#include <ship_data_model.h>
#include "net_signalk_http.h"
#include "net_http.h"

extern ship_data_t shipDataModel;

#ifdef __cplusplus
extern "C" {
#endif

  void getVesselInfo() {
    String signalk_http_host = preferences.getString(SK_HTTP_HOST_PREF);
    int signalk_http_port = preferences.getInt(SK_HTTP_PORT_PREF);
    if (signalk_http_host.length() > 0 && signalk_http_port > 0) {
      String api = String("http://") += signalk_http_host += String(":") += String(signalk_http_port) += "/signalk/v1/api/";
      String resp = httpGETRequest((api + "vessels/self/design/beam/value/").c_str());
      if (resp.length() > 0) {
        shipDataModel.design.beam.m = resp.toFloat();
        shipDataModel.design.beam.age = millis();
      }
      resp = httpGETRequest((api + "vessels/self/design/airHeight/value/").c_str());
      if (resp.length() > 0) {
        shipDataModel.design.air_height.m = resp.toFloat();
        shipDataModel.design.air_height.age = millis();
      }
      resp = httpGETRequest((api + "vessels/self/design/draft/value/maximum").c_str());
      if (resp.length() > 0) {
        shipDataModel.design.draft.m = resp.toFloat();
        shipDataModel.design.draft.age = millis();
      }
      resp = httpGETRequest((api + "vessels/self/design/length/value/overall").c_str());
      if (resp.length() > 0) {
        shipDataModel.design.length.m = resp.toFloat();
        shipDataModel.design.length.age = millis();
      }
      resp = httpGETRequest((api + "vessels/self/name/").c_str());
      if (resp.length() > 0 && resp.indexOf("<") < 0
          && resp.indexOf("=") < 0 && resp.indexOf(":") < 0) {
        resp.replace("\"", "");
        strncpy(shipDataModel.vessel.name, resp.c_str(), sizeof(shipDataModel.vessel.name) - 1);
      }
      resp = httpGETRequest((api + "vessels/self/mmsi/").c_str());
      if (resp.length() > 0 && resp.indexOf("<") < 0
          && resp.indexOf("=") < 0 && resp.indexOf(":") < 0) {
        resp.replace("\"", "");
        snprintf(shipDataModel.vessel.mmsi, sizeof(shipDataModel.vessel.mmsi), "%s", resp.c_str());
      }
      resp = httpGETRequest((api + "vessels/self/navigation/state/value/").c_str());
      if (resp.length() > 0) {
        resp.replace("\"", "");
        set_vessel_nav_state(resp);
      }
    }
  }

#ifdef __cplusplus
} /*extern "C"*/
#endif
