#include "ui_settings_wifi.h"
#include <WiFi.h>
#include <Preferences.h>
#include "ui_keyboard.h"
#include "net_mdns.h"
#include "esp_log.h"

static const char *TAG = "WIFI_CONFIG";

String wifi_ssid;      // Store the name of the wireless network.
String wifi_password;  // Store the password of the wireless network.
boolean settingMode;


// -----------------------------------------------------------------------------
// NVS KEY DEFINITIONS
//
// ESP32 NVS keys have a maximum length of 15 characters.
//
// The previous keys:
//
//   WIFI_HIST_SSID_0
//   WIFI_HIST_PASS_0
//
// were TOO LONG and caused:
//
//   nvs_set_str fail: KEY_TOO_LONG
//
// Keep these keys short.
// -----------------------------------------------------------------------------

static constexpr const char *KEY_SSID = "SSID";
static constexpr const char *KEY_PASS = "PASS";
static constexpr const char *KEY_HCNT = "HCNT";

static String history_ssid_key(int index)
{
    return "HSS" + String(index);
}

static String history_pass_key(int index)
{
    return "HPS" + String(index);
}

// -----------------------------------------------------------------------------
// DEBUG HELPERS
// -----------------------------------------------------------------------------

static void log_preferences_state(
    const char *context)
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

    int count =
        prefs.getInt(KEY_HCNT, 0);

    count = constrain(count, 0, 5);

    for (int i = 0; i < count; ++i) {

        String ssid_key =
            history_ssid_key(i);

        String pass_key =
            history_pass_key(i);

        String hist_ssid =
            prefs.getString(
                ssid_key.c_str(),
                "");

        String hist_pass =
            prefs.getString(
                pass_key.c_str(),
                "");
    }

    prefs.end();
}


// -----------------------------------------------------------------------------
// SAVE CURRENT WIFI CREDENTIALS
// -----------------------------------------------------------------------------

static void save_wifi_history_entry(
    const String &ssid,
    const String &pass)
{
    if (ssid.length() == 0) {

        ESP_LOGW(
            TAG,
            "History save aborted: SSID is empty");

        return;
    }

    Preferences prefs;

    if (!prefs.begin("wifi-config", false)) {

        ESP_LOGE(
            TAG,
            "History: FAILED to open Preferences");

        return;
    }

    int count =
        prefs.getInt(
            KEY_HCNT,
            0);

    ESP_LOGI(
        TAG,
        "Existing history count = %d",
        count);


    // -------------------------------------------------------------------------
    // Protect against corrupted NVS.
    // -------------------------------------------------------------------------

    if (count < 0 || count > 5) {

        ESP_LOGW(
            TAG,
            "Invalid history count %d; resetting to 0",
            count);

        count = 0;
    }


    // -------------------------------------------------------------------------
    // Load the existing history into RAM.
    //
    // history[0] is the most recently used network.
    // -------------------------------------------------------------------------

    String old_ssids[5];
    String old_passwords[5];

    for (int i = 0; i < count; ++i) {

        String key_ssid =
            history_ssid_key(i);

        String key_pass =
            history_pass_key(i);

        old_ssids[i] =
            prefs.getString(
                key_ssid.c_str(),
                "");

        old_passwords[i] =
            prefs.getString(
                key_pass.c_str(),
                "");

        ESP_LOGI(
            TAG,
            "OLD HISTORY[%d]: SSID='%s'",
            i,
            old_ssids[i].c_str());
    }


    // -------------------------------------------------------------------------
    // Build the new history.
    //
    // The successfully connected network ALWAYS goes into slot 0.
    //
    // If it already existed, its old position is removed.
    // Everything else moves down by one position.
    // -------------------------------------------------------------------------

    String new_ssids[5];
    String new_passwords[5];

    int new_count = 0;


    // -------------------------------------------------------------------------
    // Slot 0 = the network that just connected successfully.
    // -------------------------------------------------------------------------

    new_ssids[0] =
        ssid;

    new_passwords[0] =
        pass;

    new_count = 1;


    // -------------------------------------------------------------------------
    // Copy the remaining historical networks.
    //
    // Skip:
    //
    //   1. Empty entries
    //   2. The SSID that was just connected
    //
    // This guarantees there are never duplicate SSIDs.
    // -------------------------------------------------------------------------

    for (int i = 0;
         i < count &&
         new_count < 5;
         ++i) {

        if (old_ssids[i].length() == 0) {
            continue;
        }

        if (old_ssids[i].equals(ssid)) {

            ESP_LOGI(
                TAG,
                "Moving existing SSID '%s' to HISTORY[0]",
                ssid.c_str());

            continue;
        }


        new_ssids[new_count] =
            old_ssids[i];

        new_passwords[new_count] =
            old_passwords[i];

        new_count++;
    }


    // -------------------------------------------------------------------------
    // Write the complete new history.
    // -------------------------------------------------------------------------

    ESP_LOGI(
        TAG,
        "Writing updated Wi-Fi history:");

    for (int i = 0;
         i < new_count;
         ++i) {

        String key_ssid =
            history_ssid_key(i);

        String key_pass =
            history_pass_key(i);

        size_t ssid_result =
            prefs.putString(
                key_ssid.c_str(),
                new_ssids[i]);

        size_t pass_result =
            prefs.putString(
                key_pass.c_str(),
                new_passwords[i]);


        if (ssid_result == 0) {

            ESP_LOGE(
                TAG,
                "FAILED to save HISTORY[%d] SSID",
                i);

        }

        if (pass_result == 0) {

            ESP_LOGE(
                TAG,
                "FAILED to save HISTORY[%d] PASSWORD",
                i);

        }


        ESP_LOGI(
            TAG,
            "HISTORY[%d]: SSID='%s'",
            i,
            new_ssids[i].c_str());
    }


    // -------------------------------------------------------------------------
    // Remove stale entries from the old history.
    //
    // Example:
    //
    // Old:
    //   0 TULIP
    //   1 GL-SFT1200-fad
    //   2 OLD-NETWORK
    //
    // New:
    //   0 TULIP
    //   1 GL-SFT1200-fad
    //
    // HSS2/HPS2 must be removed.
    // -------------------------------------------------------------------------

    for (int i = new_count;
         i < count;
         ++i) {

        String key_ssid =
            history_ssid_key(i);

        String key_pass =
            history_pass_key(i);

        prefs.remove(
            key_ssid.c_str());

        prefs.remove(
            key_pass.c_str());
    }


    // -------------------------------------------------------------------------
    // Store the new count.
    // -------------------------------------------------------------------------

    size_t count_result =
        prefs.putInt(
            KEY_HCNT,
            new_count);

    if (count_result == 0) {

        ESP_LOGE(
            TAG,
            "FAILED to save history count");

    } else {

        ESP_LOGI(
            TAG,
            "History count updated: %d",
            new_count);
    }


    prefs.end();


    // -------------------------------------------------------------------------
    // Verify the history after writing.
    // -------------------------------------------------------------------------

    Preferences verify;

    if (!verify.begin("wifi-config", true)) {

        ESP_LOGE(
            TAG,
            "History verification: FAILED to reopen Preferences");

        return;
    }


    int verify_count =
        verify.getInt(
            KEY_HCNT,
            0);

    ESP_LOGI(
        TAG,
        "Verified history count = %d",
        verify_count);


    if (verify_count > 5) {
        verify_count = 5;
    }


    for (int i = 0;
         i < verify_count;
         ++i) {

        String key_ssid =
            history_ssid_key(i);

        String key_pass =
            history_pass_key(i);

        String verify_ssid =
            verify.getString(
                key_ssid.c_str(),
                "");

        String verify_pass =
            verify.getString(
                key_pass.c_str(),
                "");

        ESP_LOGI(
            TAG,
            "VERIFIED HISTORY[%d]: SSID='%s' PASSWORD='%s'",
            i,
            verify_ssid.c_str(),
            verify_pass.c_str());
    }


    verify.end();
}


static void save_current_wifi_credentials(
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

        return;
    }

    Preferences prefs;

    if (!prefs.begin("wifi-config", false)) {

        ESP_LOGE(
            TAG,
            "FAILED to open wifi-config Preferences");

        return;
    }


    // -------------------------------------------------------------------------
    // Save current SSID.
    // -------------------------------------------------------------------------

    size_t ssid_result =
        prefs.putString(
            KEY_SSID,
            ssid);

    if (ssid_result == 0) {

        ESP_LOGE(
            TAG,
            "FAILED to save SSID");

    } else {

        ESP_LOGI(
            TAG,
            "SSID saved successfully: '%s' (%u bytes)",
            ssid.c_str(),
            (unsigned)ssid_result);
    }


    // -------------------------------------------------------------------------
    // Save current password.
    // -------------------------------------------------------------------------

    size_t pass_result =
        prefs.putString(
            KEY_PASS,
            password);

    if (pass_result == 0) {

        ESP_LOGE(
            TAG,
            "FAILED to save PASSWORD");

    } else {

        ESP_LOGI(
            TAG,
            "PASSWORD saved successfully: '%s' (%u bytes)",
            password.c_str(),
            (unsigned)pass_result);
    }


    prefs.end();


    // -------------------------------------------------------------------------
    // Verify immediately after writing.
    // -------------------------------------------------------------------------

    Preferences verify;

    if (verify.begin("wifi-config", true)) {

        String verify_ssid =
            verify.getString(
                KEY_SSID,
                "");

        String verify_pass =
            verify.getString(
                KEY_PASS,
                "");

        if (verify_ssid != ssid) {

            ESP_LOGE(
                TAG,
                "SSID VERIFY FAILED!");

        }

        if (verify_pass != password) {

            ESP_LOGE(
                TAG,
                "PASSWORD VERIFY FAILED!");

        }

        verify.end();

    } else {

        ESP_LOGE(
            TAG,
            "Could not reopen Preferences for verification");
    }


    // Dump everything currently stored.
    log_preferences_state(
        "AFTER SAVE");

    ESP_LOGI(
        TAG,
        "==================================================");
}


// -----------------------------------------------------------------------------
// LOAD WIFI HISTORY
// -----------------------------------------------------------------------------

static int load_wifi_history(
    String *ssids,
    String *passwords,
    int max_items)
{
    ESP_LOGI(
        TAG,
        "Loading Wi-Fi history");

    Preferences prefs;

    if (!prefs.begin("wifi-config", true)) {

        ESP_LOGE(
            TAG,
            "FAILED to open Preferences for history read");

        return 0;
    }

    int count =
        prefs.getInt(
            KEY_HCNT,
            0);

    ESP_LOGI(
        TAG,
        "Stored history count = %d",
        count);

    if (count < 0 || count > 5) {

        ESP_LOGW(
            TAG,
            "Invalid stored history count: %d",
            count);

        count = 0;
    }

    count =
        min(
            count,
            max_items);


    for (int i = 0; i < count; ++i) {

        String key_ssid =
            history_ssid_key(i);

        String key_pass =
            history_pass_key(i);

        ssids[i] =
            prefs.getString(
                key_ssid.c_str(),
                "");

        passwords[i] =
            prefs.getString(
                key_pass.c_str(),
                "");
    }

    prefs.end();

    return count;
}


// -----------------------------------------------------------------------------
// RESTORE CONFIGURATION
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

    // -------------------------------------------------------------------------
    // Load current credentials from NVS.
    // -------------------------------------------------------------------------

    Preferences prefs;

    if (!prefs.begin("wifi-config", true)) {

        ESP_LOGE(
            TAG,
            "FAILED to open wifi-config Preferences");

        return false;
    }

    String stored_current_ssid =
        prefs.getString(
            KEY_SSID,
            "");

    String stored_current_password =
        prefs.getString(
            KEY_PASS,
            "");

    int stored_count =
        prefs.getInt(
            KEY_HCNT,
            0);

    prefs.end();


    ESP_LOGI(
        TAG,
        "CURRENT NVS CREDENTIALS:");

    ESP_LOGI(
        TAG,
        "  SSID     = '%s'",
        stored_current_ssid.c_str());

    ESP_LOGI(
        TAG,
        "  PASSWORD = '%s'",
        stored_current_password.c_str());

    ESP_LOGI(
        TAG,
        "  HISTORY COUNT = %d",
        stored_count);


    // -------------------------------------------------------------------------
    // Load Wi-Fi history.
    // -------------------------------------------------------------------------

    String historical_ssids[5];
    String historical_passwords[5];

    int history_count =
        load_wifi_history(
            historical_ssids,
            historical_passwords,
            5);


    ESP_LOGI(
        TAG,
        "Loaded %d historical Wi-Fi networks",
        history_count);

    for (int i = 0; i < history_count; ++i) {

        ESP_LOGI(
            TAG,
            "HISTORY[%d]: SSID='%s'",
            i,
            historical_ssids[i].c_str());
    }


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
    // IMPORTANT:
    //
    // Scan for networks BEFORE selecting credentials.
    //
    // We must not blindly call WiFi.begin() for every historical network.
    // Only a saved SSID that is actually visible should be selected.
    // -------------------------------------------------------------------------

    ESP_LOGI(
        TAG,
        "Scanning for available Wi-Fi networks...");

    WiFi.disconnect();

    delay(100);

    int scan_count =
        WiFi.scanNetworks();

    ESP_LOGI(
        TAG,
        "Wi-Fi scan found %d networks",
        scan_count);


    // -------------------------------------------------------------------------
    // First preference:
    //
    // If the CURRENT stored SSID is visible, use it.
    //
    // This preserves the user's current network when it is available.
    // -------------------------------------------------------------------------

    int selected_history_index = -1;
    int selected_scan_index = -1;


    if (stored_current_ssid.length() > 0) {

        for (int scan_index = 0;
             scan_index < scan_count;
             ++scan_index) {

            String found_ssid =
                WiFi.SSID(scan_index);

            ESP_LOGI(
                TAG,
                "SCAN[%d]: SSID='%s' RSSI=%d",
                scan_index,
                found_ssid.c_str(),
                WiFi.RSSI(scan_index));

            if (found_ssid.equals(stored_current_ssid)) {

                ESP_LOGI(
                    TAG,
                    "Current stored SSID is visible: '%s'",
                    stored_current_ssid.c_str());

                selected_scan_index =
                    scan_index;

                break;
            }
        }
    }


    // -------------------------------------------------------------------------
    // If the current SSID is not visible, search Wi-Fi history.
    //
    // History order is retained:
    //
    //   HISTORY[0] = most recently used
    //   HISTORY[1] = next most recent
    //   ...
    //
    // The first historical SSID that appears in the scan wins.
    // -------------------------------------------------------------------------

    if (selected_scan_index < 0) {

        ESP_LOGI(
            TAG,
            "Current stored SSID is not visible.");

        ESP_LOGI(
            TAG,
            "Searching Wi-Fi history for a visible network...");


        for (int history_index = 0;
             history_index < history_count;
             ++history_index) {

            if (historical_ssids[history_index].length() == 0) {
                continue;
            }


            for (int scan_index = 0;
                 scan_index < scan_count;
                 ++scan_index) {

                String found_ssid =
                    WiFi.SSID(scan_index);

                if (found_ssid.equals(
                        historical_ssids[history_index])) {

                    selected_history_index =
                        history_index;

                    selected_scan_index =
                        scan_index;

                    ESP_LOGI(
                        TAG,
                        "==================================================");

                    ESP_LOGI(
                        TAG,
                        "VISIBLE SAVED NETWORK FOUND");

                    ESP_LOGI(
                        TAG,
                        "History index = %d",
                        history_index);

                    ESP_LOGI(
                        TAG,
                        "Scan index = %d",
                        scan_index);

                    ESP_LOGI(
                        TAG,
                        "SSID = '%s'",
                        historical_ssids[history_index].c_str());

                    ESP_LOGI(
                        TAG,
                        "RSSI = %d",
                        WiFi.RSSI(scan_index));

                    ESP_LOGI(
                        TAG,
                        "==================================================");

                    break;
                }
            }


            if (selected_scan_index >= 0) {
                break;
            }
        }
    }


    // -------------------------------------------------------------------------
    // We found a visible network.
    // -------------------------------------------------------------------------

    if (selected_scan_index >= 0) {

        String selected_ssid =
            WiFi.SSID(selected_scan_index);

        String selected_password;


        // ---------------------------------------------------------------------
        // If this SSID exists in history, use its historical password.
        // ---------------------------------------------------------------------

        for (int i = 0;
             i < history_count;
             ++i) {

            if (historical_ssids[i].equals(
                    selected_ssid)) {

                selected_password =
                    historical_passwords[i];

                selected_history_index =
                    i;

                ESP_LOGI(
                    TAG,
                    "Using password from HISTORY[%d]",
                    i);

                break;
            }
        }


        // ---------------------------------------------------------------------
        // If the SSID wasn't in history, but is the current stored SSID,
        // use the current stored password.
        // ---------------------------------------------------------------------

        if (selected_password.length() == 0 &&
            selected_ssid.equals(
                stored_current_ssid)) {

            selected_password =
                stored_current_password;

            ESP_LOGI(
                TAG,
                "Using password from current NVS credentials");
        }


        // ---------------------------------------------------------------------
        // Set the active credentials.
        // ---------------------------------------------------------------------

        wifi_ssid =
            selected_ssid;

        wifi_password =
            selected_password;


        ESP_LOGI(
            TAG,
            "==================================================");

        ESP_LOGI(
            TAG,
            "SELECTED WIFI NETWORK");

        ESP_LOGI(
            TAG,
            "SSID='%s'",
            wifi_ssid.c_str());

        ESP_LOGI(
            TAG,
            "History index=%d",
            selected_history_index);

        ESP_LOGI(
            TAG,
            "Calling WiFi.begin() ONCE");

        ESP_LOGI(
            TAG,
            "==================================================");


        // ---------------------------------------------------------------------
        // Only attempt the network that was actually found.
        // ---------------------------------------------------------------------

        WiFi.begin(
            wifi_ssid.c_str(),
            wifi_password.c_str());


        WiFi.scanDelete();

        return true;
    }


    // -------------------------------------------------------------------------
    // No saved network is currently visible.
    //
    // Do NOT start WiFi.begin() on an arbitrary historical network.
    //
    // Leave the credentials available for the caller, but report that there
    // was no visible saved network.
    // -------------------------------------------------------------------------

    ESP_LOGW(
        TAG,
        "==================================================");

    ESP_LOGW(
        TAG,
        "NO SAVED WIFI NETWORK FOUND IN SCAN");

    ESP_LOGW(
        TAG,
        "Current stored SSID='%s'",
        stored_current_ssid.c_str());

    ESP_LOGW(
        TAG,
        "No WiFi.begin() will be issued.");

    ESP_LOGW(
        TAG,
        "Entering Wi-Fi setup mode.");

    ESP_LOGW(
        TAG,
        "==================================================");


    wifi_ssid =
        stored_current_ssid;

    wifi_password =
        stored_current_password;


    WiFi.scanDelete();

    return false;
}


// -----------------------------------------------------------------------------
// RESET WIFI SETTINGS
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
        "RESET CURRENT WIFI SETTINGS");

    // Show what is currently stored before removing it.
    log_preferences_state(
        "BEFORE CURRENT WIFI RESET");

    Preferences prefs;

    if (!prefs.begin("wifi-config", false)) {

        ESP_LOGE(
            TAG,
            "FAILED to open wifi-config Preferences");

        return;
    }

    // -------------------------------------------------------------------------
    // Remove ONLY the currently configured Wi-Fi credentials.
    //
    // Do NOT call prefs.clear().
    //
    // Wi-Fi history remains untouched:
    //   HCNT
    //   HSS0 ... HSS4
    //   HPS0 ... HPS4
    // -------------------------------------------------------------------------

    bool ssid_removed =
        prefs.remove(KEY_SSID);

    bool pass_removed =
        prefs.remove(KEY_PASS);

    prefs.end();


    ESP_LOGI(
        TAG,
        "Current SSID key removed: %s",
        ssid_removed ? "YES" : "NO");

    ESP_LOGI(
        TAG,
        "Current password key removed: %s",
        pass_removed ? "YES" : "NO");


    // Clear the in-memory credentials too.
    wifi_ssid = "";
    wifi_password = "";


    // -------------------------------------------------------------------------
    // Verify that CURRENT credentials are gone while HISTORY remains.
    // -------------------------------------------------------------------------

    log_preferences_state(
        "AFTER CURRENT WIFI RESET");

    ESP_LOGW(
        TAG,
        "Current Wi-Fi credentials cleared.");
    
    ESP_LOGW(
        TAG,
        "Wi-Fi history was NOT cleared.");

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

    (*on_connected)();

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

        String ssid =
            WiFi.SSID(i);

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
            "PASSWORD = '%s'",
            pass.c_str());

        ESP_LOGI(
            TAG,
            "PASSWORD LENGTH = %u",
            (unsigned)pass.length());


        save_current_wifi_credentials(
            ssid,
            pass);


        lv_msgbox(
            "Password submitted");

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


    String title =
        " Wi-Fi Password: " +
        WiFi.SSID(i).substring(0, 9) +
        "...";

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

    if (code == LV_EVENT_CLICKED) {

        int n =
            (int)(intptr_t)
            lv_event_get_user_data(e);

#ifdef ENABLE_SCREEN_SERVER

        screenServer0();

#endif

        lv_connect_wifi_win(n);
    }
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

    int n =
        WiFi.scanNetworks();

    ESP_LOGI(
        TAG,
        "Wi-Fi scan found %d networks",
        n);

    lv_list_wifi(
        lv_screen_active(),
        n);


    // Allow reconnect while in setup mode.
    restoreConfig();


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

            // wifi_connected(on_connected);
            // Restart for a clean attempt.
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


    preferences.begin(
        "wifi-config",
        false);


    if (restoreConfig()) {

        ESP_LOGI(
            TAG,
            "restoreConfig() returned TRUE");

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
            "Stored credentials did not connect");
    }


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