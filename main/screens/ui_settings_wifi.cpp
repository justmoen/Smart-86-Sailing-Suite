#include "ui_settings_wifi.h"
#include <WiFi.h>
#include <Preferences.h>
#include "ui_keyboard.h"
#include "net_mdns.h"
#include "esp_log.h"

static const char *TAG = "WIFI_CONFIG";

static lv_obj_t *list_wifi = NULL;
static String wifi_selected_ssid;

String wifi_ssid;
String wifi_password;
boolean settingMode;


// -----------------------------------------------------------------------------
// NVS
//
// Only ONE Wi-Fi configuration is stored.
//
// ESP32 NVS keys have a maximum length of 15 characters.
// -----------------------------------------------------------------------------

static constexpr const char *KEY_SSID = "SSID";
static constexpr const char *KEY_PASS = "PASS";


// -----------------------------------------------------------------------------
// DEBUG
// -----------------------------------------------------------------------------

static void log_preferences_state(const char *context)
{
    Preferences prefs;

    if (!prefs.begin("wifi-config", true)) {
        ESP_LOGE(
            TAG,
            "[%s] FAILED to open Preferences namespace",
            context);
        return;
    }

    String stored_ssid =
        prefs.getString(KEY_SSID, "");

    String stored_pass =
        prefs.getString(KEY_PASS, "");

    ESP_LOGI(
        TAG,
        "[%s] Stored SSID='%s'",
        context,
        stored_ssid.c_str());

    ESP_LOGI(
        TAG,
        "[%s] Stored password length=%u",
        context,
        (unsigned)stored_pass.length());

    prefs.end();
}


// -----------------------------------------------------------------------------
// SAVE WIFI CREDENTIALS
//
// This replaces whatever Wi-Fi credentials were previously stored.
//
// There is deliberately NO history.
// -----------------------------------------------------------------------------

static bool save_current_wifi_credentials(
    const String &ssid,
    const String &password)
{
    ESP_LOGI(
        TAG,
        "==================================================");

    ESP_LOGI(
        TAG,
        "save_current_wifi_credentials()");

    if (ssid.length() == 0) {

        ESP_LOGW(
            TAG,
            "SAVE ABORTED: SSID is empty");

        return false;
    }

    Preferences prefs;

    if (!prefs.begin("wifi-config", false)) {

        ESP_LOGE(
            TAG,
            "FAILED to open wifi-config Preferences");

        return false;
    }

    size_t ssid_result =
        prefs.putString(
            KEY_SSID,
            ssid);

    size_t pass_result =
        prefs.putString(
            KEY_PASS,
            password);

    prefs.end();

    if (ssid_result == 0) {

        ESP_LOGE(
            TAG,
            "FAILED to save SSID");

        return false;
    }

    if (pass_result == 0) {

        ESP_LOGE(
            TAG,
            "FAILED to save PASSWORD");

        return false;
    }

    ESP_LOGI(
        TAG,
        "SSID saved: '%s'",
        ssid.c_str());

    ESP_LOGI(
        TAG,
        "Password saved: %u bytes",
        (unsigned)password.length());


    // -------------------------------------------------------------------------
    // Verify immediately.
    // -------------------------------------------------------------------------

    Preferences verify;

    if (!verify.begin("wifi-config", true)) {

        ESP_LOGE(
            TAG,
            "Could not reopen Preferences for verification");

        return false;
    }

    String verify_ssid =
        verify.getString(
            KEY_SSID,
            "");

    String verify_pass =
        verify.getString(
            KEY_PASS,
            "");

    verify.end();


    bool valid =
        (verify_ssid == ssid &&
         verify_pass == password);

    if (!valid) {

        ESP_LOGE(
            TAG,
            "Wi-Fi credential verification FAILED");

        ESP_LOGE(
            TAG,
            "SSID verification: %s",
            verify_ssid == ssid ? "OK" : "FAILED");

        ESP_LOGE(
            TAG,
            "Password verification: %s",
            verify_pass == password ? "OK" : "FAILED");

    } else {

        ESP_LOGI(
            TAG,
            "Wi-Fi credentials verified successfully");
    }


    log_preferences_state(
        "AFTER SAVE");

    ESP_LOGI(
        TAG,
        "==================================================");

    return valid;
}


// -----------------------------------------------------------------------------
// LOAD WIFI CREDENTIALS
//
// Returns true only when an SSID has been stored.
//
// The password may legitimately be empty for an open network.
// -----------------------------------------------------------------------------

static bool load_wifi_credentials(
    String &ssid,
    String &password)
{
    Preferences prefs;

    if (!prefs.begin("wifi-config", true)) {

        ESP_LOGE(
            TAG,
            "FAILED to open Preferences");

        return false;
    }

    ssid =
        prefs.getString(
            KEY_SSID,
            "");

    password =
        prefs.getString(
            KEY_PASS,
            "");

    prefs.end();


    ESP_LOGI(
        TAG,
        "Loaded saved Wi-Fi configuration:");

    ESP_LOGI(
        TAG,
        "  SSID='%s'",
        ssid.c_str());

    ESP_LOGI(
        TAG,
        "  Password length=%u",
        (unsigned)password.length());


    return ssid.length() > 0;
}


// -----------------------------------------------------------------------------
// RESTORE CONFIGURATION
//
// There is only one saved network.
//
// IMPORTANT:
//
// This function does NOT scan.
//
// The caller simply attempts the saved credentials. If the connection fails,
// setupMode() performs a scan and lets the user choose a new network.
// -----------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

boolean restoreConfig()
{
    ESP_LOGI(
        TAG,
        "==================================================");

    ESP_LOGI(
        TAG,
        "restoreConfig()");

    String stored_ssid;
    String stored_password;

    if (!load_wifi_credentials(
            stored_ssid,
            stored_password)) {

        ESP_LOGW(
            TAG,
            "No saved Wi-Fi credentials");

        wifi_ssid = "";
        wifi_password = "";

        ESP_LOGI(
            TAG,
            "==================================================");

        return false;
    }


    wifi_ssid =
        stored_ssid;

    wifi_password =
        stored_password;


    // -------------------------------------------------------------------------
    // Configure Wi-Fi.
    // -------------------------------------------------------------------------

    WiFi.setMinSecurity(
        WIFI_AUTH_WEP);

    WiFi.setAutoReconnect(
        true);

    WiFi.mode(
        WIFI_STA);


    // -------------------------------------------------------------------------
    // Start the ONE saved connection attempt.
    // -------------------------------------------------------------------------

    ESP_LOGI(
        TAG,
        "Saved Wi-Fi network:");
    
    ESP_LOGI(
        TAG,
        "  SSID='%s'",
        wifi_ssid.c_str());

    ESP_LOGI(
        TAG,
        "Calling WiFi.begin() ONCE");


    WiFi.disconnect();

    delay(100);

    WiFi.begin(
        wifi_ssid.c_str(),
        wifi_password.c_str());


    ESP_LOGI(
        TAG,
        "WiFi.begin() called");


    ESP_LOGI(
        TAG,
        "==================================================");

    // TRUE means "credentials were found and connection was started".
    //
    // It does NOT mean that Wi-Fi is already connected.
    return true;
}


// -----------------------------------------------------------------------------
// RESET WIFI SETTINGS
//
// Remove the ONLY stored SSID and password.
// -----------------------------------------------------------------------------

void btnResetWiFiSettings_event(
    lv_event_t *event)
{
    (void)event;

    ESP_LOGW(
        TAG,
        "==================================================");

    ESP_LOGW(
        TAG,
        "RESET WIFI SETTINGS");

    log_preferences_state(
        "BEFORE WIFI RESET");


    Preferences prefs;

    if (!prefs.begin("wifi-config", false)) {

        ESP_LOGE(
            TAG,
            "FAILED to open wifi-config Preferences");

        return;
    }


    bool ssid_removed =
        prefs.remove(KEY_SSID);

    bool pass_removed =
        prefs.remove(KEY_PASS);

    prefs.end();


    ESP_LOGI(
        TAG,
        "SSID removed: %s",
        ssid_removed ? "YES" : "NO");

    ESP_LOGI(
        TAG,
        "Password removed: %s",
        pass_removed ? "YES" : "NO");


    wifi_ssid = "";
    wifi_password = "";
    wifi_selected_ssid = "";


    log_preferences_state(
        "AFTER WIFI RESET");


    ESP_LOGW(
        TAG,
        "Wi-Fi credentials cleared.");

    ESP_LOGW(
        TAG,
        "Restarting...");

    ESP_LOGW(
        TAG,
        "==================================================");


    delay(100);

    ESP.restart();
}


// -----------------------------------------------------------------------------
// WIFI CONNECTED
// -----------------------------------------------------------------------------

void wifi_connected(
    void (*on_connected)())
{
    ESP_LOGI(
        TAG,
        "wifi_connected()");

    if (wifi_ssid.length() > 0) {

        save_current_wifi_credentials(
            wifi_ssid,
            wifi_password);
    }

    if (on_connected != NULL) {
        (*on_connected)();
    }

    settingMode = false;
}


// -----------------------------------------------------------------------------
// SIGNAL STRENGTH
// -----------------------------------------------------------------------------

static inline int8_t dBm_to_percents(
    int8_t dBm)
{
    int quality;

    if (dBm <= -100)
        quality = 0;
    else if (dBm >= -50)
        quality = 100;
    else
        quality = 2 * (dBm + 100);

    return quality;
}


// -----------------------------------------------------------------------------
// WINDOW CLOSE
// -----------------------------------------------------------------------------

static void lv_win_close_event_cb(
    lv_event_t *e)
{
    lv_obj_t *win =
        (lv_obj_t *)lv_event_get_user_data(e);

    if (win != NULL) {
        lv_obj_delete(win);
    }
}


// -----------------------------------------------------------------------------
// MESSAGE BOX
// -----------------------------------------------------------------------------

static void event_msgbox_cb(
    lv_event_t *e)
{
    if (lv_event_get_code(e) !=
        LV_EVENT_CLICKED) {

        return;
    }

    lv_obj_t *mbox =
        (lv_obj_t *)lv_event_get_user_data(e);

    if (mbox != NULL) {
        lv_msgbox_close(mbox);
    }

    delay(100);

    ESP.restart();
}


static void lv_msgbox(
    const char *txt)
{
    lv_obj_t *mbox =
        lv_msgbox_create(NULL);

    lv_msgbox_add_title(
        mbox,
        "");

    lv_msgbox_add_text(
        mbox,
        txt);

    lv_obj_t *reboot_btn =
        lv_msgbox_add_footer_button(
            mbox,
            "Reboot");

    lv_obj_set_size(
        mbox,
        500,
        250);

    lv_obj_set_style_bg_color(
        mbox,
        lv_color_hex(0x1e1e1e),
        LV_PART_MAIN);

    lv_obj_set_style_text_color(
        mbox,
        lv_color_hex(0xffffff),
        LV_PART_MAIN);

    lv_obj_set_style_border_color(
        mbox,
        lv_color_hex(0x404040),
        LV_PART_MAIN);

    lv_obj_set_style_text_font(
        mbox,
        &lv_font_montserrat_22,
        LV_PART_MAIN);


    lv_obj_set_style_bg_color(
        reboot_btn,
        lv_color_hex(0x2a2a2a),
        LV_PART_MAIN);

    lv_obj_set_style_text_color(
        reboot_btn,
        lv_color_hex(0xffffff),
        LV_PART_MAIN);

    lv_obj_set_style_text_font(
        reboot_btn,
        &lv_font_montserrat_18,
        LV_PART_MAIN);

    lv_obj_set_size(
        reboot_btn,
        LV_SIZE_CONTENT,
        48);

    lv_obj_set_style_pad_hor(
        reboot_btn,
        24,
        LV_PART_MAIN);

    lv_obj_set_style_pad_ver(
        reboot_btn,
        4,
        LV_PART_MAIN);


    lv_obj_add_event_cb(
        reboot_btn,
        event_msgbox_cb,
        LV_EVENT_CLICKED,
        mbox);

    lv_obj_center(mbox);
}


// -----------------------------------------------------------------------------
// PASSWORD TEXTAREA EVENT
// -----------------------------------------------------------------------------

static void ta_password_event_cb(
    lv_event_t *e)
{
    lv_event_code_t code =
        lv_event_get_code(e);

    lv_obj_t *ta =
        (lv_obj_t *)lv_event_get_target(e);

    int i =
        (int)(intptr_t)
        lv_event_get_user_data(e);


    if (code == LV_EVENT_CLICKED ||
        code == LV_EVENT_FOCUSED) {

        if (kb != NULL) {

            lv_keyboard_set_textarea(
                kb,
                ta);
        }

#ifdef ENABLE_SCREEN_SERVER

        screenServer0();

#endif

    } else if (code == LV_EVENT_READY) {

        // ---------------------------------------------------------------------
        // The SSID was captured when the user selected the network.
        // ---------------------------------------------------------------------

        String ssid =
            wifi_selected_ssid;

        String pass =
            lv_textarea_get_text(ta);


        ESP_LOGI(
            TAG,
            "==================================================");

        ESP_LOGI(
            TAG,
            "PASSWORD SUBMITTED");

        ESP_LOGI(
            TAG,
            "Wi-Fi scan index = %d",
            i);

        ESP_LOGI(
            TAG,
            "SSID = '%s'",
            ssid.c_str());

        ESP_LOGI(
            TAG,
            "PASSWORD LENGTH = %u",
            (unsigned)pass.length());


        // ---------------------------------------------------------------------
        // Do not allow an empty SSID to overwrite the configuration.
        // ---------------------------------------------------------------------

        if (ssid.length() == 0) {

            ESP_LOGE(
                TAG,
                "PASSWORD SUBMISSION ABORTED: SSID is empty");

            lv_msgbox(
                "No Wi-Fi network selected");

            return;
        }


        // ---------------------------------------------------------------------
        // Save the new network.
        //
        // This completely replaces the previous network.
        // ---------------------------------------------------------------------

        if (!save_current_wifi_credentials(
                ssid,
                pass)) {

            ESP_LOGE(
                TAG,
                "Failed to save Wi-Fi credentials");

            lv_msgbox(
                "Failed to save Wi-Fi settings");

            return;
        }


        wifi_ssid =
            ssid;

        wifi_password =
            pass;


        ESP_LOGI(
            TAG,
            "New Wi-Fi configuration saved.");

        ESP_LOGI(
            TAG,
            "The previous network has been replaced.");


        lv_msgbox(
            "Wi-Fi settings saved");


        ESP_LOGI(
            TAG,
            "==================================================");
    }
}


// -----------------------------------------------------------------------------
// PASSWORD TEXTAREA
// -----------------------------------------------------------------------------

void lv_password_textarea(
    int i,
    lv_obj_t *cont)
{
    lv_obj_set_style_pad_all(
        cont,
        2,
        LV_PART_MAIN);

    lv_obj_set_style_bg_color(
        cont,
        lv_color_hex(0x1e1e1e),
        LV_PART_MAIN);

    lv_obj_set_style_border_color(
        cont,
        lv_color_hex(0x404040),
        LV_PART_MAIN);


    lv_obj_t *pwd_ta =
        lv_textarea_create(cont);

    lv_obj_set_style_text_font(
        pwd_ta,
        &lv_font_montserrat_28,
        LV_PART_MAIN);

    lv_textarea_set_text(
        pwd_ta,
        "");

    // Password masking intentionally disabled.
    // lv_textarea_set_password_mode(
    //     pwd_ta,
    //     true);

    lv_textarea_set_one_line(
        pwd_ta,
        true);

    lv_obj_set_width(
        pwd_ta,
        230);

    lv_obj_align(
        pwd_ta,
        LV_ALIGN_TOP_LEFT,
        2,
        2);


    lv_obj_set_style_bg_color(
        pwd_ta,
        lv_color_hex(0x2a2a2a),
        LV_PART_MAIN);

    lv_obj_set_style_text_color(
        pwd_ta,
        lv_color_hex(0xffffff),
        LV_PART_MAIN);

    lv_obj_set_style_border_color(
        pwd_ta,
        lv_color_hex(0x404040),
        LV_PART_MAIN);


    lv_obj_add_event_cb(
        pwd_ta,
        ta_password_event_cb,
        LV_EVENT_ALL,
        (void *)(intptr_t)i);


    kb =
        lv_keyboard2(cont);

    lv_display_t *display =
        lv_display_get_default();

    lv_coord_t display_width =
        lv_display_get_horizontal_resolution(
            display);

    lv_coord_t display_height =
        lv_display_get_vertical_resolution(
            display);

    lv_obj_align(
        kb,
        LV_ALIGN_BOTTOM_MID,
        0,
        0);

    lv_obj_set_size(
        kb,
        display_width - 4,
        (display_height / 2) + 48);

    lv_keyboard_set_textarea(
        kb,
        pwd_ta);

    lv_obj_add_state(
        pwd_ta,
        LV_STATE_FOCUSED);
}


// -----------------------------------------------------------------------------
// CONNECT WIFI WINDOW
// -----------------------------------------------------------------------------

void lv_connect_wifi_win(
    int i)
{
    lv_obj_t *win =
        lv_win_create(
            lv_screen_active());

    lv_obj_set_style_text_font(
        win,
        &lv_font_montserrat_28,
        LV_PART_MAIN);

    lv_obj_set_style_bg_color(
        win,
        lv_color_hex(0x1e1e1e),
        LV_PART_MAIN);

    lv_obj_set_style_text_color(
        win,
        lv_color_hex(0xffffff),
        LV_PART_MAIN);

    lv_obj_set_style_border_color(
        win,
        lv_color_hex(0x404040),
        LV_PART_MAIN);


    // -------------------------------------------------------------------------
    // The SSID was captured when the list button was pressed.
    // -------------------------------------------------------------------------

    String display_ssid =
        wifi_selected_ssid;

    String title;

    if (display_ssid.length() > 9) {

        title =
            " Wi-Fi Password: " +
            display_ssid.substring(0, 9) +
            "...";

    } else {

        title =
            " Wi-Fi Password: " +
            display_ssid;
    }


    lv_win_add_title(
        win,
        title.c_str());


    lv_obj_t *btn =
        lv_win_add_button(
            win,
            LV_SYMBOL_CLOSE,
            28);

    lv_obj_set_style_bg_color(
        btn,
        lv_color_hex(0xff4444),
        LV_PART_MAIN);

    lv_obj_set_style_text_color(
        btn,
        lv_color_hex(0xffffff),
        LV_PART_MAIN);

    lv_obj_add_event_cb(
        btn,
        lv_win_close_event_cb,
        LV_EVENT_PRESSED,
        win);


    lv_obj_t *cont =
        lv_win_get_content(win);

    lv_obj_set_style_bg_color(
        cont,
        lv_color_hex(0x1e1e1e),
        LV_PART_MAIN);


    lv_password_textarea(
        i,
        cont);
}


// -----------------------------------------------------------------------------
// WIFI LIST EVENT
// -----------------------------------------------------------------------------

static void event_handler_wifi(
    lv_event_t *e)
{
    lv_event_code_t code =
        lv_event_get_code(e);

    if (code != LV_EVENT_CLICKED) {
        return;
    }

    int n =
        (int)(intptr_t)
        lv_event_get_user_data(e);


    // -------------------------------------------------------------------------
    // Capture the SSID immediately.
    //
    // This is the only point where the scan index is needed.
    // -------------------------------------------------------------------------

    wifi_selected_ssid =
        WiFi.SSID(n);


    ESP_LOGI(
        TAG,
        "Wi-Fi selection:");

    ESP_LOGI(
        TAG,
        "  scan index=%d",
        n);

    ESP_LOGI(
        TAG,
        "  SSID='%s'",
        wifi_selected_ssid.c_str());


#ifdef ENABLE_SCREEN_SERVER

    screenServer0();

#endif


    lv_connect_wifi_win(n);
}


// -----------------------------------------------------------------------------
// WIFI LIST
// -----------------------------------------------------------------------------

void lv_list_wifi(
    lv_obj_t *parent,
    int num)
{
    list_wifi =
        lv_list_create(parent);

    lv_obj_set_size(
        list_wifi,
        680,
        680);

    lv_obj_center(
        list_wifi);

    lv_obj_set_style_text_font(
        list_wifi,
        &lv_font_montserrat_28,
        LV_PART_MAIN);

    lv_obj_set_style_bg_color(
        list_wifi,
        lv_color_hex(0x1e1e1e),
        LV_PART_MAIN);

    lv_obj_set_style_text_color(
        list_wifi,
        lv_color_hex(0xffffff),
        LV_PART_MAIN);

    lv_obj_set_style_border_color(
        list_wifi,
        lv_color_hex(0x404040),
        LV_PART_MAIN);


    lv_list_add_text(
        list_wifi,
        "Wi-Fi Networks");

    lv_obj_t *text_label =
        lv_obj_get_child(
            list_wifi,
            0);

    if (text_label) {

        lv_obj_set_style_text_color(
            text_label,
            lv_color_hex(0xaaaaaa),
            LV_PART_MAIN);
    }


    for (int i = 0;
         i < num;
         ++i) {

        String left_text =
            String(LV_SYMBOL_WIFI) +
            "  " +
            String(
                dBm_to_percents(
                    WiFi.RSSI(i))) +
            "%";

        String ssid_text =
            ((WiFi.encryptionType(i) ==
              WIFI_AUTH_OPEN)
                ? String(
                    LV_SYMBOL_EYE_OPEN " ")
                : String("")) +
            WiFi.SSID(i);


        lv_obj_t *btn =
            lv_list_add_button(
                list_wifi,
                left_text.c_str(),
                ssid_text.c_str());


        lv_obj_set_style_text_font(
            btn,
            &lv_font_montserrat_28,
            LV_PART_MAIN);

        lv_obj_set_style_bg_color(
            btn,
            lv_color_hex(0x2a2a2a),
            LV_PART_MAIN);

        lv_obj_set_style_text_color(
            btn,
            lv_color_hex(0xffffff),
            LV_PART_MAIN);

        lv_obj_set_style_bg_color(
            btn,
            lv_color_hex(0x404040),
            LV_PART_MAIN | LV_STATE_PRESSED);

        lv_obj_set_style_border_color(
            btn,
            lv_color_hex(0x404040),
            LV_PART_MAIN);


        lv_obj_add_event_cb(
            btn,
            event_handler_wifi,
            LV_EVENT_CLICKED,
            (void *)(intptr_t)i);


        // ---------------------------------------------------------------------
        // Give the FreeRTOS idle task an opportunity to run.
        //
        // This is intentional because the LVGL watchdog previously triggered
        // while constructing this list.
        // ---------------------------------------------------------------------

        vTaskDelay(
            pdMS_TO_TICKS(1));
    }
}


// -----------------------------------------------------------------------------
// SETUP MODE
// -----------------------------------------------------------------------------

static void setupMode(
    void (*on_connected)())
{
    (void)on_connected;

    ESP_LOGI(
        TAG,
        "Entering Wi-Fi setup mode");


    WiFi.mode(
        WIFI_STA);

    WiFi.disconnect();

    delay(100);


    // -------------------------------------------------------------------------
    // ONE scan for the setup screen.
    //
    // Do not call restoreConfig() here.
    // Do not call WiFi.scanDelete() here.
    //
    // The scan results must remain available while the user selects a network.
    // -------------------------------------------------------------------------

    int n =
        WiFi.scanNetworks();


    ESP_LOGI(
        TAG,
        "Wi-Fi scan found %d networks",
        n);


    for (int i = 0;
         i < n;
         ++i) {

        ESP_LOGI(
            TAG,
            "SCAN[%d]: SSID='%s' RSSI=%d",
            i,
            WiFi.SSID(i).c_str(),
            WiFi.RSSI(i));
    }


    // -------------------------------------------------------------------------
    // Display the scan results.
    // -------------------------------------------------------------------------

    lv_list_wifi(
        lv_screen_active(),
        n);


    // -------------------------------------------------------------------------
    // When Wi-Fi connects, clean up and restart.
    //
    // The credentials were already stored when the password was submitted.
    // -------------------------------------------------------------------------

    WiFi.onEvent(
        [](WiFiEvent_t event,
           WiFiEventInfo_t info) {

            (void)event;
            (void)info;

            ESP_LOGI(
                TAG,
                "Wi-Fi STA_CONNECTED event");


            if (list_wifi != NULL) {

                lv_obj_delete(
                    list_wifi);

                list_wifi = NULL;
            }


            delay(2000);

            ESP.restart();

        },
        WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
}


// -----------------------------------------------------------------------------
// CHECK CONNECTION
// -----------------------------------------------------------------------------

boolean checkConnection()
{
    int count = 0;

    ESP_LOGI(
        TAG,
        "Checking Wi-Fi connection...");


    // 30 * 350 ms = approximately 10.5 seconds.

    while (count < 30) {

        wl_status_t status =
            WiFi.status();


        ESP_LOGI(
            TAG,
            "Wi-Fi status check %d/30: %d",
            count + 1,
            status);


        if (status ==
            WL_CONNECTED) {

            ESP_LOGI(
                TAG,
                "Wi-Fi CONNECTED");

            ESP_LOGI(
                TAG,
                "SSID='%s'",
                WiFi.SSID().c_str());

            ESP_LOGI(
                TAG,
                "IP='%s'",
                WiFi.localIP().toString().c_str());

            return true;
        }


        delay(350);

        count++;
    }


    ESP_LOGW(
        TAG,
        "Wi-Fi connection FAILED after 10.5 seconds");

    return false;
}


// -----------------------------------------------------------------------------
// START WIFI
// -----------------------------------------------------------------------------

void settingUpWiFi(
    void (*on_connected)())
{
    ESP_LOGI(
        TAG,
        "==================================================");

    ESP_LOGI(
        TAG,
        "settingUpWiFi()");


    log_preferences_state(
        "BEFORE CONNECTION");


    // -------------------------------------------------------------------------
    // Try the ONE saved network.
    // -------------------------------------------------------------------------

    if (restoreConfig()) {

        ESP_LOGI(
            TAG,
            "Saved Wi-Fi credentials found");

        if (checkConnection()) {

            ESP_LOGI(
                TAG,
                "Connection successful");

            wifi_connected(
                on_connected);

            return;
        }


        ESP_LOGW(
            TAG,
            "Saved credentials did not connect");
    }


    // -------------------------------------------------------------------------
    // No saved network, or saved network failed.
    //
    // Let the user select a new network.
    // -------------------------------------------------------------------------

    ESP_LOGI(
        TAG,
        "Entering Wi-Fi setting mode");

    settingMode = true;


    setupMode(
        on_connected);
}


#ifdef __cplusplus
}
#endif