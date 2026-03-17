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
extern Preferences preferences;

#ifdef __cplusplus
extern "C" {
#endif

  static lv_obj_t *list_wifi;
  boolean restoreConfig();
  static void btnResetWiFiSettings_event(lv_event_t *event);
  void wifi_connected(void (*on_connected)());
  static inline int8_t dBm_to_percents(int8_t dBm);
  static void lv_win_close_event_cb(lv_event_t *e);
  static void event_msgbox_cb(lv_event_t *e);
  static void lv_msgbox(const char *txt);
  static void ta_password_event_cb(lv_event_t *e);
  void lv_connect_wifi_win(int i);
  void lv_list_wifi(lv_obj_t *parent, int num);
  static void setupMode(void (*on_connected)());
  boolean checkConnection();
  void settingUpWiFi(void (*on_connected)());

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
