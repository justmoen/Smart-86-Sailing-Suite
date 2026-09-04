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
    const String &pass);


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


    // -------------------------------------------------------------------------
    // Save into Wi-Fi history.
    // -------------------------------------------------------------------------

    save_wifi_history_entry(
        ssid,
        password);


    // Dump everything currently stored.
    log_preferences_state(
        "AFTER SAVE");

    ESP_LOGI(
        TAG,
        "==================================================");
}


// -----------------------------------------------------------------------------
// WIFI HISTORY
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

    // Protect against corrupted NVS count.
    if (count < 0 || count > 5) {

        ESP_LOGW(
            TAG,
            "Invalid history count %d; resetting to 0",
            count);

        count = 0;
    }


    // -------------------------------------------------------------------------
    // Update an existing entry.
    // -------------------------------------------------------------------------

    for (int i = 0; i < count; ++i) {

        String key_ssid =
            history_ssid_key(i);

        String key_pass =
            history_pass_key(i);

        String saved_ssid =
            prefs.getString(
                key_ssid.c_str(),
                "");

        ESP_LOGI(
            TAG,
            "Checking history[%d]: SSID='%s'",
            i,
            saved_ssid.c_str());

        if (saved_ssid.equals(ssid)) {

            ESP_LOGI(
                TAG,
                "Existing SSID found in history[%d]",
                i);

            size_t result =
                prefs.putString(
                    key_pass.c_str(),
                    pass);

            if (result == 0) {

                ESP_LOGE(
                    TAG,
                    "FAILED to update history[%d] password",
                    i);

            } else {

                ESP_LOGI(
                    TAG,
                    "Updated history[%d] password: '%s'",
                    i,
                    pass.c_str());
            }

            prefs.end();

            return;
        }
    }


    // -------------------------------------------------------------------------
    // Keep a maximum of five historical networks.
    // -------------------------------------------------------------------------

    if (count >= 5) {

        ESP_LOGI(
            TAG,
            "History full; shifting entries");

        for (int i = 1; i < count; ++i) {

            String destination_ssid_key =
                history_ssid_key(i - 1);

            String destination_pass_key =
                history_pass_key(i - 1);

            String source_ssid_key =
                history_ssid_key(i);

            String source_pass_key =
                history_pass_key(i);

            String next_ssid =
                prefs.getString(
                    source_ssid_key.c_str(),
                    "");

            String next_pass =
                prefs.getString(
                    source_pass_key.c_str(),
                    "");

            prefs.putString(
                destination_ssid_key.c_str(),
                next_ssid);

            prefs.putString(
                destination_pass_key.c_str(),
                next_pass);
        }

        count = 4;
    }


    // -------------------------------------------------------------------------
    // Add new entry.
    // -------------------------------------------------------------------------

    int slot = count;

    String key_ssid =
        history_ssid_key(slot);

    String key_pass =
        history_pass_key(slot);

    size_t ssid_result =
        prefs.putString(
            key_ssid.c_str(),
            ssid);

    size_t pass_result =
        prefs.putString(
            key_pass.c_str(),
            pass);


    if (ssid_result == 0) {

        ESP_LOGE(
            TAG,
            "FAILED to save history[%d] SSID",
            slot);

    } else {

        ESP_LOGI(
            TAG,
            "History[%d] SSID saved (%u bytes)",
            slot,
            (unsigned)ssid_result);
    }


    if (pass_result == 0) {

        ESP_LOGE(
            TAG,
            "FAILED to save history[%d] PASSWORD",
            slot);

    } else {

        ESP_LOGI(
            TAG,
            "History[%d] PASSWORD saved (%u bytes)",
            slot,
            (unsigned)pass_result);
    }


    // Update count only after credentials were written.
    int new_count =
        count + 1;

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

    Preferences prefs;

    if (!prefs.begin("wifi-config", true)) {

        ESP_LOGE(
            TAG,
            "FAILED to open wifi-config Preferences");

        return false;
    }

    wifi_ssid =
        prefs.getString(
            KEY_SSID,
            "");

    wifi_password =
        prefs.getString(
            KEY_PASS,
            "");

    int stored_count =
        prefs.getInt(
            KEY_HCNT,
            0);

    ESP_LOGI(
        TAG,
        "CURRENT NVS CREDENTIALS:");
    ESP_LOGI(
        TAG,
        "  SSID     = '%s'",
        wifi_ssid.c_str());
    ESP_LOGI(
        TAG,
        "  PASSWORD = '%s'",
        wifi_password.c_str());
    ESP_LOGI(
        TAG,
        "  HISTORY COUNT = %d",
        stored_count);

    prefs.end();


    if (wifi_ssid.length() == 0) {

        ESP_LOGW(
            TAG,
            "No stored Wi-Fi SSID");

        return false;
    }


    // -------------------------------------------------------------------------
    // Load history.
    // -------------------------------------------------------------------------

    String historical_ssids[5];
    String historical_passwords[5];

    int history_count =
        load_wifi_history(
            historical_ssids,
            historical_passwords,
            5);


    // -------------------------------------------------------------------------
    // Restore the password associated with the current SSID.
    //
    // This is important because the historical entry is the authoritative
    // password for a network if it exists.
    // -------------------------------------------------------------------------

    for (int i = 0; i < history_count; ++i) {

        if (historical_ssids[i].length() == 0) {
            continue;
        }

        if (historical_ssids[i].equals(wifi_ssid)) {

            ESP_LOGI(
                TAG,
                "Found current SSID in history[%d]",
                i);

            ESP_LOGI(
                TAG,
                "History password = '%s'",
                historical_passwords[i].c_str());

            wifi_password =
                historical_passwords[i];

            break;
        }
    }

    WiFi.setMinSecurity(
        WIFI_AUTH_WEP);

    WiFi.setAutoReconnect(
        true);

    WiFi.mode(
        WIFI_STA);


    // -------------------------------------------------------------------------
    // Build candidate list.
    // -------------------------------------------------------------------------

    String ssids[6];
    String passwords[6];

    ssids[0] =
        wifi_ssid;

    passwords[0] =
        wifi_password;

    int candidate_count = 1;


    for (int i = 0;
         i < history_count &&
         candidate_count < 6;
         ++i) {

        if (historical_ssids[i].length() == 0 ||
            historical_ssids[i].equals(wifi_ssid)) {

            continue;
        }

        ssids[candidate_count] =
            historical_ssids[i];

        passwords[candidate_count] =
            historical_passwords[i];

        candidate_count++;
    }


    // -------------------------------------------------------------------------
    // Attempt each candidate.
    // -------------------------------------------------------------------------

    ESP_LOGI(
        TAG,
        "Wi-Fi candidate count = %d",
        candidate_count);

    for (int i = 0;
         i < candidate_count;
         ++i) {

        if (ssids[i].length() == 0) {
            continue;
        }

        wifi_ssid =
            ssids[i];

        wifi_password =
            passwords[i];

        WiFi.begin(
            wifi_ssid.c_str(),
            wifi_password.c_str());
    }


    ESP_LOGI(
        TAG,
        "restoreConfig() complete");

    ESP_LOGI(
        TAG,
        "==================================================");

    return wifi_ssid.length() > 0;
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