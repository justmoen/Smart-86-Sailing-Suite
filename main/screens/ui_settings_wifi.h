#ifndef UI_SETTINGS_WIFI_H
#define UI_SETTINGS_WIFI_H

#ifndef CONFIG_ESP32_WIFI_ENABLE_WPA3_SAE
#warning "No WPA3 support."
#endif

#include "ui_screens.h"
#include <Arduino.h>
#include <Preferences.h>

extern String wifi_ssid;
extern String wifi_password;
extern boolean settingMode;

#ifdef __cplusplus
extern "C" {
#endif

  static lv_obj_t *list_wifi;
  boolean restoreConfig();
  void wifi_connected(void (*on_connected)());
  void lv_connect_wifi_win(int i);
  void lv_list_wifi(lv_obj_t *parent, int num);
  boolean checkConnection();
  void settingUpWiFi(void (*on_connected)());

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
