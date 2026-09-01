#include "ui_settings_wifi.h"
#include <WiFi.h>
#include "ui_keyboard.h"
#include "net_mdns.h"

String wifi_ssid;      // Store the name of the wireless network.
String wifi_password;  // Store the password of the wireless network.
boolean settingMode;

static void save_wifi_history_entry(const String& ssid, const String& pass);

static void save_current_wifi_credentials(const String& ssid, const String& password) {
  if (ssid.length() == 0) return;
  Preferences prefs;
  prefs.begin("wifi-config", false);
  prefs.putString("WIFI_SSID", ssid);
  prefs.putString("WIFI_PASSWD", password);
  prefs.end();
  save_wifi_history_entry(ssid, password);
}

static void save_wifi_history_entry(const String& ssid, const String& pass) {
  if (ssid.length() == 0) return;

  Preferences prefs;
  prefs.begin("wifi-config", false);

  int count = prefs.getInt("WIFI_HISTORY_COUNT", 0);
  for (int i = 0; i < count; ++i) {
    String key_ssid = "WIFI_HISTORY_SSID_" + String(i);
    String key_pass = "WIFI_HISTORY_PASS_" + String(i);
    String saved_ssid = prefs.getString(key_ssid.c_str(), "");
    if (saved_ssid.equals(ssid)) {
      prefs.putString(key_pass.c_str(), pass);
      prefs.end();
      return;
    }
  }

  if (count >= 5) {
    for (int i = 1; i < count; ++i) {
      String key_ssid = "WIFI_HISTORY_SSID_" + String(i - 1);
      String key_pass = "WIFI_HISTORY_PASS_" + String(i - 1);
      String next_ssid = prefs.getString(("WIFI_HISTORY_SSID_" + String(i)).c_str(), "");
      String next_pass = prefs.getString(("WIFI_HISTORY_PASS_" + String(i)).c_str(), "");
      prefs.putString(key_ssid.c_str(), next_ssid);
      prefs.putString(key_pass.c_str(), next_pass);
    }
    count = 5;
    count--;
  }

  int slot = count;
  prefs.putString(("WIFI_HISTORY_SSID_" + String(slot)).c_str(), ssid);
  prefs.putString(("WIFI_HISTORY_PASS_" + String(slot)).c_str(), pass);
  prefs.putInt("WIFI_HISTORY_COUNT", count + 1);
  prefs.end();
}

static int load_wifi_history(String* ssids, String* passwords, int max_items) {
  Preferences prefs;
  prefs.begin("wifi-config", true);
  int count = prefs.getInt("WIFI_HISTORY_COUNT", 0);
  count = min(count, max_items);
  for (int i = 0; i < count; ++i) {
    ssids[i] = prefs.getString(("WIFI_HISTORY_SSID_" + String(i)).c_str(), "");
    passwords[i] = prefs.getString(("WIFI_HISTORY_PASS_" + String(i)).c_str(), "");
  }
  prefs.end();
  return count;
}

#ifdef __cplusplus
extern "C" {
#endif

  boolean restoreConfig() {  // Check whether there is wifi configuration information storage, if there is return 1, if no return 0.
    Preferences prefs;
    prefs.begin("wifi-config", true);  // Open in read-only mode
    wifi_ssid = prefs.getString("WIFI_SSID", "");
    wifi_password = prefs.getString("WIFI_PASSWD", "");
    prefs.end();

    if (wifi_ssid.length() == 0) {
      return false;
    }

    String historical_ssids[5];
    String historical_passwords[5];
    int history_count = load_wifi_history(historical_ssids, historical_passwords, 5);

    for (int i = 0; i < history_count; ++i) {
      if (historical_ssids[i].length() == 0) continue;
      if (historical_ssids[i].equals(wifi_ssid)) {
        wifi_password = historical_passwords[i];
        break;
      }
    }

    WiFi.setMinSecurity(WIFI_AUTH_WEP);
    WiFi.setAutoReconnect(true);
    WiFi.mode(WIFI_STA);

    // Try the current SSID first, then any prior networks in order as fallback.
    String ssids[6];
    String passwords[6];
    ssids[0] = wifi_ssid;
    passwords[0] = wifi_password;
    int candidate_count = 1;

    for (int i = 0; i < history_count && candidate_count < 6; ++i) {
      if (historical_ssids[i].length() == 0 || historical_ssids[i].equals(wifi_ssid)) continue;
      ssids[candidate_count] = historical_ssids[i];
      passwords[candidate_count] = historical_passwords[i];
      candidate_count++;
    }

    for (int i = 0; i < candidate_count; ++i) {
      if (ssids[i].length() == 0) continue;
      wifi_ssid = ssids[i];
      wifi_password = passwords[i];
      WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    }

    return wifi_ssid.length() > 0;
  }

  void btnResetWiFiSettings_event(lv_event_t *event) {
    preferences.begin("wifi-config", false);  // Open in read-write mode
    preferences.remove("WIFI_SSID");
    preferences.remove("WIFI_PASSWD");
    preferences.end();
    ESP.restart();
  }

  void wifi_connected(void (*on_connected)()) {
    if (wifi_ssid.length() > 0) {
      save_current_wifi_credentials(wifi_ssid, wifi_password);
    }
    (*on_connected)();
    settingMode = false;  // Turn off setting mode.
  }

  static inline int8_t dBm_to_percents(int8_t dBm) {
    int quality;
    // dBm to Quality:
    if (dBm <= -100)
      quality = 0;
    else if (dBm >= -50)
      quality = 100;
    else
      quality = 2 * (dBm + 100);
    return quality;
  }

  static void lv_win_close_event_cb(lv_event_t *e) {
    lv_obj_del((lv_obj_t *)lv_event_get_user_data(e));
  }

  static void event_msgbox_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_current_target(e);
    lv_msgbox_close(obj);
    delay(100);
    ESP.restart();
  }

  static void lv_msgbox(const char *txt) {
    static const char *btns[] PROGMEM = { "Reboot", "" };
    lv_obj_t *mbox = lv_msgbox_create(NULL, "", "Password submitted", btns, true);
    
    // Set messagebox size to prevent text wrapping
    lv_obj_set_size(mbox, 500, 250);
    lv_obj_set_style_text_font(mbox, &lv_font_montserrat_22, LV_PART_MAIN);
    
    // Dark mode for messagebox
    lv_obj_set_style_bg_color(mbox, lv_color_hex(0x1e1e1e), LV_PART_MAIN);
    lv_obj_set_style_text_color(mbox, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_border_color(mbox, lv_color_hex(0x404040), LV_PART_MAIN);
    
    // Style messagebox buttons
    uint32_t btn_count = 0;
    lv_obj_t * btn_obj;
    while((btn_obj = lv_msgbox_get_btns(mbox)) && (btn_obj = lv_obj_get_child(btn_obj, btn_count++))) {
        lv_obj_set_style_bg_color(btn_obj, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        lv_obj_set_style_text_color(btn_obj, lv_color_hex(0xffffff), LV_PART_MAIN);
        lv_obj_set_style_text_font(btn_obj, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_size(btn_obj, LV_SIZE_CONTENT, 48);
        lv_obj_set_style_pad_hor(btn_obj, 24, LV_PART_MAIN); 
        lv_obj_set_style_pad_ver(btn_obj, 4, LV_PART_MAIN);
    }
    
    lv_obj_add_event_cb(mbox, event_msgbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_center(mbox);
  }

  static void ta_password_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    int i = (int)lv_event_get_user_data(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
      /*Focus on the clicked text area*/
      if (kb != NULL) lv_keyboard_set_textarea(kb, ta);
#ifdef ENABLE_SCREEN_SERVER
      // (not for production)
      screenServer0();
#endif
    } else if (code == LV_EVENT_READY) {
      String ssid = WiFi.SSID(i);
      String pass = lv_textarea_get_text(ta);
      save_current_wifi_credentials(ssid, pass);
      lv_msgbox(pass.c_str());
    }
  }

  void lv_password_textarea(int i, lv_obj_t *cont) {
    /*Create the password box*/
    lv_obj_set_style_pad_all(cont, 2, LV_PART_MAIN);
    
    // Dark mode for container
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x1e1e1e), LV_PART_MAIN);
    lv_obj_set_style_border_color(cont, lv_color_hex(0x404040), LV_PART_MAIN);
    
    lv_obj_t *pwd_ta = lv_textarea_create(cont);
    lv_obj_set_style_text_font(pwd_ta, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_textarea_set_text(pwd_ta, "");
    //lv_textarea_set_password_mode(pwd_ta, true);
    lv_textarea_set_one_line(pwd_ta, true);
    lv_obj_set_width(pwd_ta, 230);
    lv_obj_align(pwd_ta, LV_ALIGN_TOP_LEFT, 2, 2);
    
    // Dark mode for textarea
    lv_obj_set_style_bg_color(pwd_ta, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_text_color(pwd_ta, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_border_color(pwd_ta, lv_color_hex(0x404040), LV_PART_MAIN);
    
    lv_obj_add_event_cb(pwd_ta, ta_password_event_cb, LV_EVENT_ALL, (void *)i);

    /*Create a keyboard*/
    kb = lv_keyboard2(cont);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_size(kb, LV_HOR_RES - 4, (LV_VER_RES / 2) + 48);

    lv_keyboard_set_textarea(kb, pwd_ta); /*Focus it on one of the text areas to start*/
    lv_obj_add_state(pwd_ta, LV_STATE_FOCUSED);
  }

  void lv_connect_wifi_win(int i) {
    lv_obj_t *win = lv_win_create(lv_scr_act(), 28);
    lv_obj_set_style_text_font(win, &lv_font_montserrat_28, LV_PART_MAIN);
    
    // Dark mode for window
    lv_obj_set_style_bg_color(win, lv_color_hex(0x1e1e1e), LV_PART_MAIN);
    lv_obj_set_style_text_color(win, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_border_color(win, lv_color_hex(0x404040), LV_PART_MAIN);
    
    lv_win_add_title(win, (String(" Wi-Fi Password: ") += String(WiFi.SSID(i)).substring(0, 9) += "...").c_str());
    lv_obj_t *btn = lv_win_add_btn(win, LV_SYMBOL_CLOSE, 28);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xff4444), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_add_event_cb(btn, lv_win_close_event_cb, LV_EVENT_PRESSED, win);
    lv_obj_t *cont = lv_win_get_content(win);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x1e1e1e), LV_PART_MAIN);
    lv_password_textarea(i, cont);
  }

  static void event_handler_wifi(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
      int n = (int)lv_event_get_user_data(e);
#ifdef ENABLE_SCREEN_SERVER
      // (not for production)
      screenServer0();
#endif
      lv_connect_wifi_win(n);
    }
  }

  void lv_list_wifi(lv_obj_t *parent, int num) {
    /*Create a list*/
    list_wifi = lv_list_create(parent);
    lv_obj_set_size(list_wifi, 680, 680);
    lv_obj_center(list_wifi);
    lv_obj_set_style_text_font(list_wifi, &lv_font_montserrat_28, LV_PART_MAIN);
    
    // Dark mode styling
    lv_obj_set_style_bg_color(list_wifi, lv_color_hex(0x1e1e1e), LV_PART_MAIN);
    lv_obj_set_style_text_color(list_wifi, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_border_color(list_wifi, lv_color_hex(0x404040), LV_PART_MAIN);

    /*Add buttons to the list*/
    lv_obj_t *btn;

    lv_list_add_text(list_wifi, "Wi-Fi Networks");
    lv_obj_t *text_label = lv_obj_get_child(list_wifi, 0);
    if (text_label) {
      lv_obj_set_style_text_color(text_label, lv_color_hex(0xaaaaaa), LV_PART_MAIN);
    }

    for (int i = 0; i < num; ++i) {
      btn = lv_list_add_btn(list_wifi, (String(LV_SYMBOL_WIFI) += String("  ") += String(dBm_to_percents(WiFi.RSSI(i))) += "%").c_str(),
                            (((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? String(LV_SYMBOL_EYE_OPEN " ") : String("")) += WiFi.SSID(i)).c_str());

      lv_obj_set_style_text_font(btn, &lv_font_montserrat_28, LV_PART_MAIN);
      lv_obj_set_style_bg_color(btn, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
      lv_obj_set_style_text_color(btn, lv_color_hex(0xffffff), LV_PART_MAIN);
      lv_obj_set_style_bg_color(btn, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_PRESSED);
      lv_obj_set_style_border_color(btn, lv_color_hex(0x404040), LV_PART_MAIN);
      lv_obj_add_event_cb(btn, event_handler_wifi, LV_EVENT_CLICKED, (void *)i);
    }
  }

  static void setupMode(void (*on_connected)()) {
    WiFi.mode(WIFI_STA);  // Set WiFi to station mode and disconnect from an AP if it was previously connected.
    WiFi.disconnect();    // Turn off all wifi connections.
    delay(100);           // 100 ms delay.

    int n = WiFi.scanNetworks();  // return the number of networks found.

    lv_list_wifi(lv_scr_act(), n);
    restoreConfig();  // to allow reconnect while in setup mode

    WiFi.onEvent([&](WiFiEvent_t event, WiFiEventInfo_t info) {
      if (list_wifi != NULL) {
        lv_obj_del(list_wifi);
        list_wifi = NULL;
      }
      delay(2000);
      //wifi_connected(on_connected);
      ESP.restart();  // can just restart for clean attempt
    },
                 WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  }

  boolean checkConnection() {  // Check wifi connection.
    int count = 0;             // count.
    while (count < 30) {       // If you fail to connect to wifi within 30*350ms (10.5s), return false; otherwise return true.
      if (WiFi.status() == WL_CONNECTED) {
        return true;
      }
      delay(350);
      count++;
    }
    return false;
  }

  void settingUpWiFi(void (*on_connected)()) {
    preferences.begin("wifi-config", false);
    if (restoreConfig()) {      // Check if wifi configuration information has been stored.
      if (checkConnection()) {  // Check wifi connection.
        wifi_connected(on_connected);
        return;
      }
    }
    settingMode = true;  // If there is no stored wifi configuration information, turn on the setting mode.
    setupMode(on_connected);
  }

#ifdef __cplusplus
}
#endif
