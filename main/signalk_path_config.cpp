#include "signalk_path_config.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <vector>
#include <stdint.h>
#include <esp_https_ota.h>
#include <esp_crt_bundle.h>
#include <esp_system.h>
#include "net_mdns.h"
#include "net_signalk_http.h"
#include "screen_config.h"
#include "ui_manager.h"

extern const uint8_t _binary_web_admin_html_start[] asm("_binary_admin_html_start");
extern const uint8_t _binary_web_firmware_html_start[] asm("_binary_firmware_html_start");
extern const uint8_t _binary_web_signalk_config_html_start[] asm("_binary_signalk_config_html_start");
extern const uint8_t _binary_web_display_config_html_start[] asm("_binary_display_config_html_start");

static String html_escape(const String& value) {
    String escaped;
    escaped.reserve(value.length() + 16);
    for (size_t i = 0; i < value.length(); ++i) {
        switch (value.charAt(i)) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '\"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped += value.charAt(i); break;
        }
    }
    return escaped;
}

static void apply_template_value(String& html, const char* token, const String& value) {
    String placeholder = "{{";
    placeholder += token;
    placeholder += "}}";
    html.replace(placeholder, value);
}

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "v0.7.26"
#endif

constexpr const char* kPrefsNamespace = "sk-config";
constexpr const char* kGitHubRepo = "justmoen/Smart-86-Sailing-Suite";

String load_string_pref(const char* key, const char* default_val);
void save_string_pref(const char* key, const String& value);
static String get_manual_signalk_host() {
    Preferences prefs;
    prefs.begin("signalk", true);
    String value = prefs.getString(SK_MANUAL_HOST_PREF, "");
    if (value.length() == 0) {
        value = prefs.getString("signalk_manual_host", "");
        if (value.length() > 0) {
            prefs.end();
            Preferences writer;
            writer.begin("signalk", false);
            writer.putString(SK_MANUAL_HOST_PREF, value);
            writer.remove("signalk_manual_host");
            writer.end();
        }
    }
    prefs.end();
    return value;
}
static int get_manual_signalk_port() {
    Preferences prefs;
    prefs.begin("signalk", true);
    int value = prefs.getInt(SK_MANUAL_PORT_PREF, 3000);
    if (value == 3000 && prefs.isKey("signalk_manual_port")) {
        value = prefs.getInt("signalk_manual_port", 3000);
        if (value > 0) {
            prefs.end();
            Preferences writer;
            writer.begin("signalk", false);
            writer.putInt(SK_MANUAL_PORT_PREF, value);
            writer.remove("signalk_manual_port");
            writer.end();
        }
    }
    prefs.end();
    return value;
}
static String get_saved_release_pref(const char* key, const String& fallback) {
    return load_string_pref(key, fallback.c_str());
}

static void set_saved_release_pref(const char* key, const String& value) {
    save_string_pref(key, value);
}
static bool is_valid_nvs_key(const char* key) {
    if (key == nullptr) return false;
    size_t len = strlen(key);
    return len > 0 && len <= 15;
}

static String normalize_release_tag(const String& tag) {
    String value = tag;
    value.trim();
    if (value.length() > 0 && value.charAt(0) == 'v') {
        value = value.substring(1);
    }
    return value;
}

static bool version_is_newer(const String& candidate, const String& current) {
    String a = normalize_release_tag(candidate);
    String b = normalize_release_tag(current);
    if (a.length() == 0) return false;
    if (b.length() == 0) return true;

    std::vector<int> current_parts;
    std::vector<int> candidate_parts;
    String current_token;
    String candidate_token;

    auto append_tokens = [](const String& text, std::vector<int>& out) {
        String token;
        for (size_t i = 0; i < text.length(); ++i) {
            char ch = text.charAt(i);
            if (isdigit(ch)) {
                token += ch;
            } else if (token.length() > 0) {
                out.push_back(token.toInt());
                token = "";
            }
        }
        if (token.length() > 0) {
            out.push_back(token.toInt());
        }
    };

    append_tokens(a, candidate_parts);
    append_tokens(b, current_parts);

    size_t limit = std::max(candidate_parts.size(), current_parts.size());
    if (limit == 0) return false;
    while (candidate_parts.size() < limit) candidate_parts.push_back(0);
    while (current_parts.size() < limit) current_parts.push_back(0);

    for (size_t i = 0; i < limit; ++i) {
        if (candidate_parts[i] > current_parts[i]) return true;
        if (candidate_parts[i] < current_parts[i]) return false;
    }
    return false;
}

static String http_get_text(const String& url) {
    HTTPClient client;
    client.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    client.setTimeout(15000);
    client.setUserAgent("Smart86SailingSuite");
    client.begin(url);
    int code = client.GET();
    String payload = "";
    if (code >= 200 && code < 300) {
        payload = client.getString();
    }
    client.end();
    return payload;
}

static bool fetch_release_history(
    std::vector<String>& tags,
    std::vector<String>& download_urls,
    String& error_message
) {
    tags.clear();
    download_urls.clear();

    String payload = http_get_text(
        String("https://api.github.com/repos/") +
        kGitHubRepo +
        "/releases"
    );

    if (payload.length() == 0) {
        error_message = "Unable to reach GitHub Releases.";
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (err) {
        error_message = "GitHub API response was invalid.";
        return false;
    }

    if (!doc.is<JsonArray>()) {
        error_message = "GitHub API returned an unexpected format.";
        return false;
    }

    JsonArray releases = doc.as<JsonArray>();

    for (JsonVariant release : releases) {
        if (!release["tag_name"].is<String>()) {
            continue;
        }

        String tag = release["tag_name"].as<String>();
        tag.trim();

        if (tag.length() == 0) {
            continue;
        }

        String firmware_url = "";

        /*
         * Do not assume that the first GitHub release asset
         * is our firmware.
         *
         * Only accept the exact firmware filename used by
         * Smart-86-Sailing-Suite.
         */
        if (release["assets"].is<JsonArray>()) {
            JsonArray assets = release["assets"].as<JsonArray>();

            for (JsonVariant asset : assets) {
                if (!asset["name"].is<String>()) {
                    continue;
                }

                String asset_name = asset["name"].as<String>();

                if (!asset_name.equals("sailing_suite.bin")) {
                    continue;
                }

                if (asset["browser_download_url"].is<String>()) {
                    firmware_url =
                        asset["browser_download_url"].as<String>();
                }

                break;
            }
        }

        tags.push_back(tag);
        download_urls.push_back(firmware_url);

        if (tags.size() >= 5) {
            break;
        }
    }

    if (tags.empty()) {
        error_message = "No public GitHub releases were found.";
        return false;
    }

    error_message = "";
    return true;
}

static String get_current_firmware_version() {
    return String(FIRMWARE_VERSION);
}

static String resolve_release_asset_url(
    const String& tag,
    const String& fallback_download_url = ""
) {
    /*
     * A requested release must resolve to the firmware asset
     * belonging to that exact release.
     *
     * Do not use firmware_latest_url as a fallback because
     * that URL may belong to another release.
     *
     * The fallback argument is intentionally ignored for OTA
     * safety. It is retained in the function signature so that
     * other existing callers do not need to change immediately.
     */
    (void)fallback_download_url;

    if (tag.length() == 0) {
        return "";
    }

    String api_url =
        String("https://api.github.com/repos/") +
        kGitHubRepo +
        "/releases/tags/" +
        tag;

    Serial.println();
    Serial.println("===== GITHUB RELEASE LOOKUP =====");
    Serial.println("Requested release: " + tag);
    Serial.println("API URL: " + api_url);

    String payload = http_get_text(api_url);

    if (payload.length() == 0) {
        Serial.println("GitHub release lookup returned no data.");
        return "";
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (err) {
        Serial.println(
            "GitHub release JSON parse failed: " +
            String(err.c_str())
        );
        return "";
    }

    if (!doc["assets"].is<JsonArray>()) {
        Serial.println("GitHub release contains no assets array.");
        return "";
    }

    JsonArray assets = doc["assets"].as<JsonArray>();

    for (JsonVariant asset : assets) {
        if (!asset["name"].is<String>()) {
            continue;
        }

        String asset_name = asset["name"].as<String>();

        Serial.println("GitHub asset: " + asset_name);

        if (!asset_name.equals("sailing_suite.bin")) {
            continue;
        }

        if (!asset["browser_download_url"].is<String>()) {
            Serial.println(
                "sailing_suite.bin has no browser_download_url."
            );
            return "";
        }

        String url =
            asset["browser_download_url"].as<String>();

        if (url.length() == 0) {
            return "";
        }

        Serial.println(
            "Selected firmware asset: " + asset_name
        );
        Serial.println(
            "Selected firmware URL: " + url
        );
        Serial.println("================================");
        Serial.println();

        return url;
    }

    Serial.println(
        "ERROR: sailing_suite.bin was not found in release " +
        tag
    );
    Serial.println("================================");
    Serial.println();

    return "";
}

static bool perform_firmware_ota(
    const String& tag,
    String& error_message
) {
    /*
     * Resolve the firmware from the requested release itself.
     *
     * This deliberately does NOT use the cached
     * firmware_latest_url value.
     */
    String download_url = resolve_release_asset_url(tag);

    if (download_url.length() == 0) {
        error_message =
            "No sailing_suite.bin firmware asset was found "
            "for release " +
            tag +
            ".";

        return false;
    }

    if (download_url.indexOf("http") != 0) {
        error_message =
            "The firmware URL for release " +
            tag +
            " is not valid.";

        return false;
    }

    Serial.println();
    Serial.println("========================================");
    Serial.println("         FIRMWARE OTA START");
    Serial.println("========================================");
    Serial.println("Requested release: " + tag);
    Serial.println("Firmware URL:");
    Serial.println(download_url);
    Serial.println("========================================");

    esp_http_client_config_t client_config = {};

    client_config.url = download_url.c_str();
    client_config.timeout_ms = 30000;
    client_config.keep_alive_enable = true;

    /*
     * GitHub/cloud HTTP response headers can be large.
     * Keep the 8 KB RX buffer that was added previously.
     */
    client_config.buffer_size = 8192;
    client_config.buffer_size_tx = 2048;

    client_config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_https_ota_config_t ota_config = {};

    ota_config.http_config = &client_config;
    ota_config.bulk_flash_erase = false;
    ota_config.partial_http_download = false;
    ota_config.max_http_request_size = 0;

    Serial.println("Starting esp_https_ota()...");

    esp_err_t err = esp_https_ota(&ota_config);

    if (err != ESP_OK) {
        error_message =
            "OTA failed for " +
            tag +
            ": " +
            String(esp_err_to_name(err));

        Serial.println();
        Serial.println("========================================");
        Serial.println("         FIRMWARE OTA FAILED");
        Serial.println("========================================");
        Serial.println(error_message);
        Serial.println("========================================");
        Serial.println();

        return false;
    }

    Serial.println();
    Serial.println("========================================");
    Serial.println("       FIRMWARE OTA SUCCESS");
    Serial.println("========================================");
    Serial.println("Release: " + tag);
    Serial.println("Image successfully written.");
    Serial.println("Boot partition has been selected.");
    Serial.println("========================================");
    Serial.println();

    error_message =
        "Firmware update staged successfully. Rebooting now...";

    return true;
}

signalk_path_config_t config;  // Global default-initialized, runtime prefs override in load_config_from_preferences()


WebServer web_server(80);
bool web_server_started = false;

// Helper to parse URL-encoded form data from request body
String extractFormValue(const String& body, const String& key) {
    String searchKey = key + "=";
    int startIdx = body.indexOf(searchKey);
    if (startIdx == -1) return "";
    
    startIdx += searchKey.length();
    int endIdx = body.indexOf("&", startIdx);
    if (endIdx == -1) endIdx = body.length();
    
    String value = body.substring(startIdx, endIdx);
    
    // URL decode: replace + with space, %XX with character
    value.replace("+", " ");
    int pos = 0;
    while ((pos = value.indexOf("%", pos)) != -1 && pos + 2 < (int)value.length()) {
        char hex[3];
        hex[0] = value[pos + 1];
        hex[1] = value[pos + 2];
        hex[2] = '\0';
        char decodedChar = (char)strtol(hex, NULL, 16);
        value = value.substring(0, pos) + decodedChar + value.substring(pos + 3);
        pos++;
    }
    return value;
}

// Load float from preferences with default
float load_float_pref(const char* key, float default_val) {
    if (!is_valid_nvs_key(key)) return default_val;
    Preferences prefs;
    prefs.begin(kPrefsNamespace, true);
    float val = prefs.getFloat(key, default_val);
    prefs.end();
    return val;
}

// Save float to preferences
void save_float_pref(const char* key, float value) {
    if (!is_valid_nvs_key(key)) return;
    Preferences prefs;
    prefs.begin(kPrefsNamespace, false);
    prefs.putFloat(key, value);
    prefs.end();
}

// Load bool from preferences with default
bool load_bool_pref(const char* key, bool default_val) {
    if (!is_valid_nvs_key(key)) return default_val;
    Preferences prefs;
    prefs.begin(kPrefsNamespace, true);
    bool val = prefs.getBool(key, default_val);
    prefs.end();
    return val;
}

static bool prefs_has_key(const char* key) {
    if (!is_valid_nvs_key(key)) return false;
    Preferences prefs;
    prefs.begin(kPrefsNamespace, true);
    bool exists = prefs.isKey(key);
    prefs.end();
    return exists;
}

bool load_bool_pref_legacy(const char* key, const char* legacy_key, bool default_val) {
    if (!is_valid_nvs_key(key)) return default_val;
    if (prefs_has_key(key)) {
        return load_bool_pref(key, default_val);
    }
    if (!is_valid_nvs_key(legacy_key)) return default_val;
    Preferences prefs;
    prefs.begin(kPrefsNamespace, true);
    bool val = prefs.getBool(legacy_key, default_val);
    prefs.end();
    return val;
}

// Save bool to preferences
void save_bool_pref(const char* key, bool value) {
    if (!is_valid_nvs_key(key)) return;
    Preferences prefs;
    prefs.begin(kPrefsNamespace, false);
    prefs.putBool(key, value);
    prefs.end();
}

// Load int from preferences with default
int load_int_pref(const char* key, int default_val) {
    if (!is_valid_nvs_key(key)) return default_val;
    Preferences prefs;
    prefs.begin(kPrefsNamespace, true);
    int val = prefs.getInt(key, default_val);
    prefs.end();
    return val;
}

int load_int_pref_legacy(const char* key, const char* legacy_key, int default_val) {
    if (!is_valid_nvs_key(key)) return default_val;
    if (prefs_has_key(key)) {
        int val = load_int_pref(key, default_val);
        ESP_LOGI("WS", "[DEBUG] load_int_pref_legacy key='%s' -> type=int, value=%d (from primary key)\n", key, val);
        return val;
    }
    if (!is_valid_nvs_key(legacy_key)) return default_val;
    Preferences prefs;
    prefs.begin(kPrefsNamespace, true);
    int val = prefs.getInt(legacy_key, default_val);
    prefs.end();
    ESP_LOGI("WS", "[DEBUG] load_int_pref_legacy key='%s' legacy='%s' -> type=int, value=%d (from legacy key)\n", key, legacy_key, val);
    return val;
}

// Save int to preferences
void save_int_pref(const char* key, int value) {
    if (!is_valid_nvs_key(key)) return;
    Preferences prefs;
    prefs.begin(kPrefsNamespace, false);
    prefs.putInt(key, value);
    prefs.end();
}

// Load string from preferences with default
String load_string_pref(const char* key, const char* default_val) {
    if (!is_valid_nvs_key(key)) return String(default_val);
    Preferences prefs;
    prefs.begin(kPrefsNamespace, true);
    String val = prefs.getString(key, default_val);
    prefs.end();
    return val;
}

// Save string to preferences
void save_string_pref(const char* key, const String& value) {
    if (!is_valid_nvs_key(key)) return;
    // Check if value has changed
    String current = load_string_pref(key, "___nonexistent___");
    if (value == current) {
        return;  // No change, skip write
    }
    Preferences prefs;
    prefs.begin(kPrefsNamespace, false);
    prefs.putString(key, value);
    prefs.end();
}

void load_config_from_preferences() {
    // Load Signal K paths
    config.navigation_rate_of_turn = load_string_pref("nav_rot", "navigation.rateOfTurn");
    config.navigation_heading_magnetic = load_string_pref("nav_hdg", "navigation.headingMagnetic");
    config.navigation_position = load_string_pref("nav_pos", "navigation.position");
    config.navigation_speed_over_ground = load_string_pref("nav_sog", "navigation.speedOverGround");
    config.navigation_speed_through_water = load_string_pref("nav_stw", "navigation.speedThroughWater");
    config.navigation_course_over_ground_true = load_string_pref("nav_cog", "navigation.courseOverGroundTrue");
    config.navigation_course_rhumbline_cross_track_error = load_string_pref("nav_xte", "navigation.courseRhumbline.crossTrackError");
    config.navigation_course_rhumbline_bearing_track_true = load_string_pref("nav_brg", "navigation.courseRhumbline.bearingTrackTrue");
    config.navigation_course_rhumbline_next_point_distance = load_string_pref("nav_dist", "navigation.courseRhumbline.nextPoint.distance");
    config.navigation_course_rhumbline_next_point_velocity_made_good = load_string_pref("nav_vmg", "navigation.courseRhumbline.nextPoint.velocityMadeGood");
    config.navigation_state = load_string_pref("nav_state", "navigation.state");
    config.navigation_attitude_roll = load_string_pref("nav_roll", "navigation.attitude.roll");
    config.navigation_attitude_pitch = load_string_pref("nav_pitch", "navigation.attitude.pitch");

    config.environment_wind_angle_apparent = load_string_pref("env_waa", "environment.wind.angleApparent");
    config.environment_wind_angle_true_ground = load_string_pref("env_watg", "environment.wind.angleTrueGround");
    config.environment_wind_angle_true_water = load_string_pref("env_watw", "environment.wind.angleTrueWater");
    config.environment_wind_speed_apparent = load_string_pref("env_wsa", "environment.wind.speedApparent");
    config.environment_wind_speed_over_ground = load_string_pref("env_wsog", "environment.wind.speedOverGround");
    config.environment_wind_speed_true = load_string_pref("env_wst", "environment.wind.speedTrue");
    config.environment_depth_below_keel = load_string_pref("env_dbk", "environment.depth.belowKeel");
    config.environment_depth_below_transducer = load_string_pref("env_dbt", "environment.depth.belowTransducer");
    config.environment_depth_below_surface = load_string_pref("env_dbs", "environment.depth.belowSurface");
    config.environment_outside_pressure = load_string_pref("env_press", "environment.outside.pressure");
    config.environment_outside_humidity = load_string_pref("env_humid", "environment.outside.humidity");
    config.environment_outside_temperature = load_string_pref("env_temp", "environment.outside.temperature");
    config.environment_outside_illuminance = load_string_pref("env_illum", "environment.outside.illuminance");

    config.steering_rudder_angle = load_string_pref("steer_rudder", "steering.rudderAngle");

    config.vessel_design_beam_api = load_string_pref("vessel_beam", "vessels.self.design.beam.value");
    config.vessel_design_air_height_api = load_string_pref("vessel_air", "vessels.self.design.airHeight.value");
    config.vessel_design_draft_api = load_string_pref("vessel_draft", "vessels.self.design.draft.value.maximum");
    config.vessel_design_length_api = load_string_pref("vessel_len", "vessels.self.design.length.value.overall");
    config.vessel_name_api = load_string_pref("vessel_name", "vessels.self.name");
    config.vessel_mmsi_api = load_string_pref("vessel_mmsi", "vessels.self.mmsi");
    config.vessel_navigation_state_api = load_string_pref("vessel_nav", "vessels.self.navigation.state.value");

    // Load engine Signal K paths (parameterized per engine)
    config.num_engines = load_int_pref("num_engines", 1);
    if (config.num_engines < 1) config.num_engines = 1;
    if (config.num_engines > 8) config.num_engines = 8;
    for (int i = 0; i < config.num_engines && i < 8; i++) {
        String key = String("eng_path_") + i;
        String default_path = String("propulsion.engines.") + i;
        config.engine_paths[i] = load_string_pref(key.c_str(), default_path.c_str());
    }
    
    // Load engine screen ID configurations
    config.engine_screen_1_id = load_int_pref("eng_scr_1_id", 0);
    config.engine_screen_2_id = load_int_pref("eng_scr_2_id", 1);
    if (config.engine_screen_1_id < 0) config.engine_screen_1_id = 0;
    if (config.engine_screen_1_id > 7) config.engine_screen_1_id = 7;
    if (config.engine_screen_2_id < 0) config.engine_screen_2_id = 1;
    if (config.engine_screen_2_id > 7) config.engine_screen_2_id = 7;

    // Load tank Signal K paths (parameterized per tank)
    config.num_tanks = load_int_pref("num_tanks", 4);  // Default to 4 tanks
    if (config.num_tanks < 1) config.num_tanks = 1;
    if (config.num_tanks > 8) config.num_tanks = 8;
    
    for (int i = 0; i < 8; i++) {
        String key = String("tank_path_") + i;
        String default_path = String("tanks.fluid.") + i + ".currentLevel";
        config.tank_paths[i] = load_string_pref(key.c_str(), default_path.c_str());
    }

    // Load numeric thresholds
    config.engine_oil_pressure_enabled = load_bool_pref("eng_oil_enabled", true);
    config.engine_top_left_enabled = load_bool_pref_legacy("eng_tl_en", "eng_top_left_enabled", true);
    int top_left_metric = load_int_pref_legacy("eng_tl_met", "eng_top_left_metric", (int)EngineTopLeftMetric::SOG);
    if (top_left_metric < 0 || top_left_metric > 1) top_left_metric = (int)EngineTopLeftMetric::SOG;
    config.engine_top_left_metric = static_cast<EngineTopLeftMetric>(top_left_metric);
    config.engine_top_right_enabled = load_bool_pref_legacy("eng_tr_en", "eng_top_right_enabled", true);
    int top_right_metric = load_int_pref_legacy("eng_tr_met", "eng_top_right_metric", (int)EngineTopRightMetric::AlternatorVoltage);
    if (top_right_metric < 0 || top_right_metric > 1) top_right_metric = (int)EngineTopRightMetric::AlternatorVoltage;
    config.engine_top_right_metric = static_cast<EngineTopRightMetric>(top_right_metric);
    config.engine_oil_pressure_min = load_float_pref("eng_oil_min", 10.0f);
    config.engine_oil_pressure_max = load_float_pref("eng_oil_max", 60.0f);
    config.engine_temp_redline = load_float_pref("eng_temp_red", 100.0f);

    // Load chart durations
    config.depth_chart_duration = load_int_pref("depth_chart_min", 10);
    config.speed_chart_duration = load_int_pref("speed_chart_min", 30);
    
    // Load unit preferences
    int dist_unit = load_int_pref("dist_unit", 0);  // 0=Meters, 1=Feet
    config.distance_unit = (dist_unit == 0) ? DistanceUnit::Meters : DistanceUnit::Feet;
    
    int temp_unit = load_int_pref("temp_unit", 0);  // 0=Celsius, 1=Fahrenheit
    config.temperature_unit = (temp_unit == 0) ? TemperatureUnit::Celsius : TemperatureUnit::Fahrenheit;

    Preferences prefs;
    prefs.begin(kPrefsNamespace, true);

    config.screen_config_count = prefs.getInt("screen_count", 0);

    for (int i = 0; i < config.screen_config_count && i < MAX_SCREENS_CONFIG; i++) {
        String key_id = "scr_id_" + String(i);
        String key_en = "scr_en_" + String(i);

        String id = prefs.getString(key_id.c_str(), "");
        bool enabled = prefs.getBool(key_en.c_str(), true);

        if (id.length() > 0) {
            strcpy(config.screens[i].id, id.c_str());
            config.screens[i].enabled = enabled;
        }
    }

    prefs.end();
}

void save_all_config_to_preferences() {
    // Save Signal K paths
    save_string_pref("nav_rot", config.navigation_rate_of_turn);
    save_string_pref("nav_hdg", config.navigation_heading_magnetic);
    save_string_pref("nav_pos", config.navigation_position);
    save_string_pref("nav_sog", config.navigation_speed_over_ground);
    save_string_pref("nav_stw", config.navigation_speed_through_water);
    save_string_pref("nav_cog", config.navigation_course_over_ground_true);
    save_string_pref("nav_xte", config.navigation_course_rhumbline_cross_track_error);
    save_string_pref("nav_brg", config.navigation_course_rhumbline_bearing_track_true);
    save_string_pref("nav_dist", config.navigation_course_rhumbline_next_point_distance);
    save_string_pref("nav_vmg", config.navigation_course_rhumbline_next_point_velocity_made_good);
    save_string_pref("nav_state", config.navigation_state);
    save_string_pref("nav_roll", config.navigation_attitude_roll);
    save_string_pref("nav_pitch", config.navigation_attitude_pitch);

    save_string_pref("env_waa", config.environment_wind_angle_apparent);
    save_string_pref("env_watg", config.environment_wind_angle_true_ground);
    save_string_pref("env_watw", config.environment_wind_angle_true_water);
    save_string_pref("env_wsa", config.environment_wind_speed_apparent);
    save_string_pref("env_wsog", config.environment_wind_speed_over_ground);
    save_string_pref("env_wst", config.environment_wind_speed_true);
    save_string_pref("env_dbk", config.environment_depth_below_keel);
    save_string_pref("env_dbt", config.environment_depth_below_transducer);
    save_string_pref("env_dbs", config.environment_depth_below_surface);
    save_string_pref("env_press", config.environment_outside_pressure);
    save_string_pref("env_humid", config.environment_outside_humidity);
    save_string_pref("env_temp", config.environment_outside_temperature);
    save_string_pref("env_illum", config.environment_outside_illuminance);

    save_string_pref("steer_rudder", config.steering_rudder_angle);

    save_string_pref("vessel_beam", config.vessel_design_beam_api);
    save_string_pref("vessel_air", config.vessel_design_air_height_api);
    save_string_pref("vessel_draft", config.vessel_design_draft_api);
    save_string_pref("vessel_len", config.vessel_design_length_api);
    save_string_pref("vessel_name", config.vessel_name_api);
    save_string_pref("vessel_mmsi", config.vessel_mmsi_api);
    save_string_pref("vessel_nav", config.vessel_navigation_state_api);

    // Save engine Signal K paths
    save_int_pref("num_engines", config.num_engines);
    for (int i = 0; i < 8; i++) {
        String key = String("eng_path_") + i;
        save_string_pref(key.c_str(), config.engine_paths[i]);
    }
    
    // Save engine screen ID configurations
    save_int_pref("eng_scr_1_id", config.engine_screen_1_id);
    save_int_pref("eng_scr_2_id", config.engine_screen_2_id);

    // Save numeric thresholds
    save_bool_pref("eng_oil_enabled", config.engine_oil_pressure_enabled);
    save_bool_pref("eng_tl_en", config.engine_top_left_enabled);
    save_bool_pref("eng_top_left_enabled", config.engine_top_left_enabled);
    save_int_pref("eng_tl_met", (int)config.engine_top_left_metric);
    save_int_pref("eng_top_left_metric", (int)config.engine_top_left_metric);
    save_bool_pref("eng_tr_en", config.engine_top_right_enabled);
    save_bool_pref("eng_top_right_enabled", config.engine_top_right_enabled);
    save_int_pref("eng_tr_met", (int)config.engine_top_right_metric);
    save_int_pref("eng_top_right_metric", (int)config.engine_top_right_metric);
    save_float_pref("eng_oil_min", config.engine_oil_pressure_min);
    save_float_pref("eng_oil_max", config.engine_oil_pressure_max);
    save_float_pref("eng_temp_red", config.engine_temp_redline);

    // Save chart durations
    save_int_pref("depth_chart_min", config.depth_chart_duration);
    save_int_pref("speed_chart_min", config.speed_chart_duration);

    // Save tank Signal K paths
    save_int_pref("num_tanks", config.num_tanks);
    for (int i = 0; i < 8; i++) {
        String key = String("tank_path_") + i;
        save_string_pref(key.c_str(), config.tank_paths[i]);
    }
    
    // Save unit preferences
    save_int_pref("dist_unit", (int)config.distance_unit);
    save_int_pref("temp_unit", (int)config.temperature_unit);

    Preferences prefs;
    prefs.begin(kPrefsNamespace, false);

    // Save count
    prefs.putInt("screen_count", config.screen_config_count);

    // Save entries
    for (int i = 0; i < config.screen_config_count; i++) {
        String key_id = "scr_id_" + String(i);
        String key_en = "scr_en_" + String(i);

        prefs.putString(key_id.c_str(), config.screens[i].id);
        prefs.putBool(key_en.c_str(), config.screens[i].enabled);
    }

    prefs.end();
}

// Export config to JSON
void export_config_to_json(JsonDocument& doc) {
    // Navigation
    JsonObject nav = doc["navigation"].to<JsonObject>();
    nav["rateOfTurn"] = config.navigation_rate_of_turn;
    nav["headingMagnetic"] = config.navigation_heading_magnetic;
    nav["position"] = config.navigation_position;
    nav["speedOverGround"] = config.navigation_speed_over_ground;
    nav["speedThroughWater"] = config.navigation_speed_through_water;
    nav["courseOverGroundTrue"] = config.navigation_course_over_ground_true;
    nav["courseRhumblineXTE"] = config.navigation_course_rhumbline_cross_track_error;
    nav["courseRhumblineBrg"] = config.navigation_course_rhumbline_bearing_track_true;
    nav["courseRhumblineDistance"] = config.navigation_course_rhumbline_next_point_distance;
    nav["courseRhumblineVMG"] = config.navigation_course_rhumbline_next_point_velocity_made_good;
    nav["state"] = config.navigation_state;
    nav["attitudeRoll"] = config.navigation_attitude_roll;
    nav["attitudePitch"] = config.navigation_attitude_pitch;

    // Environment
    JsonObject env = doc["environment"].to<JsonObject>();
    JsonObject wind = env["wind"].to<JsonObject>();
    wind["angleApparent"] = config.environment_wind_angle_apparent;
    wind["angleTrueGround"] = config.environment_wind_angle_true_ground;
    wind["angleTrueWater"] = config.environment_wind_angle_true_water;
    wind["speedApparent"] = config.environment_wind_speed_apparent;
    wind["speedOverGround"] = config.environment_wind_speed_over_ground;
    wind["speedTrue"] = config.environment_wind_speed_true;

    JsonObject depth = env["depth"].to<JsonObject>();
    depth["belowKeel"] = config.environment_depth_below_keel;
    depth["belowTransducer"] = config.environment_depth_below_transducer;
    depth["belowSurface"] = config.environment_depth_below_surface;

    JsonObject outside = env["outside"].to<JsonObject>();
    outside["pressure"] = config.environment_outside_pressure;
    outside["humidity"] = config.environment_outside_humidity;
    outside["temperature"] = config.environment_outside_temperature;
    outside["illuminance"] = config.environment_outside_illuminance;

    // Steering
    JsonObject steering = doc["steering"].to<JsonObject>();
    steering["rudderAngle"] = config.steering_rudder_angle;

    // Vessel
    JsonObject vessel = doc["vessel"].to<JsonObject>();
    JsonObject design = vessel["design"].to<JsonObject>();
    design["beamApi"] = config.vessel_design_beam_api;
    design["airHeightApi"] = config.vessel_design_air_height_api;
    design["draftApi"] = config.vessel_design_draft_api;
    design["lengthApi"] = config.vessel_design_length_api;
    vessel["nameApi"] = config.vessel_name_api;
    vessel["mmsiApi"] = config.vessel_mmsi_api;
    vessel["navigationStateApi"] = config.vessel_navigation_state_api;

    // Engine
    JsonObject engine = doc["engine"].to<JsonObject>();
    JsonObject oilPressure = engine["oilPressure"].to<JsonObject>();
    oilPressure["enabled"] = config.engine_oil_pressure_enabled;
    oilPressure["minPSI"] = config.engine_oil_pressure_min;
    oilPressure["maxPSI"] = config.engine_oil_pressure_max;
    engine["topLeftEnabled"] = config.engine_top_left_enabled;
    engine["topLeftMetric"] = (int)config.engine_top_left_metric;
    engine["topRightEnabled"] = config.engine_top_right_enabled;
    engine["topRightMetric"] = (int)config.engine_top_right_metric;
    engine["tempRedlineCelsius"] = config.engine_temp_redline;

    // Charts
    JsonObject charts = doc["charts"].to<JsonObject>();
    charts["depthChartMinutes"] = config.depth_chart_duration;
    charts["speedChartMinutes"] = config.speed_chart_duration;
}

// Handle web home page - Tree-based expandable UI
void handle_show_admin_index() {
    String html(reinterpret_cast<const char*>(_binary_web_admin_html_start));
    web_server.send(200, "text/html", html);
}

static bool is_release_skipped(const String& tag) {
    if (tag.length() == 0) {
        return false;
    }

    String skipped_tag = get_saved_release_pref("firmware_skipped_tag", "");
    return skipped_tag.length() > 0 && skipped_tag.equalsIgnoreCase(tag);
}

void handle_firmware_status() {
    JsonDocument doc;
    doc["currentVersion"] = get_current_firmware_version();
    doc["latestVersion"] = get_saved_release_pref("firmware_latest_tag", "unknown");
    doc["hasUpdate"] = false;
    doc["skipThisRelease"] = false;
    doc["message"] = "Checking GitHub releases...";

    std::vector<String> tags;
    std::vector<String> download_urls;
    String error_message;
    bool ok = fetch_release_history(tags, download_urls, error_message);

    if (ok && !tags.empty()) {
        String latest_tag = tags[0];
        String latest_downloader = download_urls[0];
        set_saved_release_pref("firmware_latest_tag", latest_tag);
        set_saved_release_pref("firmware_latest_url", latest_downloader);

        doc["latestVersion"] = latest_tag;
        doc["latestUrl"] = latest_downloader;
        doc["skipThisRelease"] = is_release_skipped(latest_tag);
        doc["hasUpdate"] = version_is_newer(latest_tag, get_current_firmware_version()) && !doc["skipThisRelease"].as<bool>();

        if (doc["skipThisRelease"].as<bool>()) {
            doc["message"] = "A newer version is available, but this release has been skipped.";
        } else if (doc["hasUpdate"].as<bool>()) {
            doc["message"] = "New release available.";
        } else {
            doc["message"] = "You are on the latest available release.";
        }

        JsonArray releases = doc["releases"].to<JsonArray>();
        for (size_t i = 0; i < tags.size(); ++i) {
            JsonObject item = releases.add<JsonObject>();
            item["tag"] = tags[i];
            item["downloadUrl"] = download_urls[i];
        }
    } else {
        doc["message"] = error_message;
        if (doc["latestVersion"].as<String>().length() == 0 || doc["latestVersion"].as<String>() == "unknown") {
            doc["latestVersion"] = "unknown";
        }
    }

    String payload;
    serializeJson(doc, payload);
    web_server.send(200, "application/json", payload);
}

void handle_firmware_skip() {
    JsonDocument req;
    String body = web_server.arg("plain");
    if (body.length() > 0) {
        deserializeJson(req, body);
    }

    String tag = req["tag"].is<String>() ? req["tag"].as<String>() : get_saved_release_pref("firmware_latest_tag", "");
    if (tag.length() == 0) {
        web_server.send(400, "application/json", R"({"status":"error","message":"No release tag was provided."})");
        return;
    }

    set_saved_release_pref("firmware_skipped_tag", tag);

    JsonDocument out;
    out["status"] = "skipped";
    out["tag"] = tag;
    out["message"] = "Release " + tag + " will be skipped on future boot checks.";
    String payload;
    serializeJson(out, payload);
    web_server.send(200, "application/json", payload);
}

void handle_firmware_update() {
    JsonDocument req;
    String body = web_server.arg("plain");
    if (body.length() > 0) {
        deserializeJson(req, body);
    }

    String requested_tag = req["tag"].is<String>() ? req["tag"].as<String>() : get_saved_release_pref("firmware_latest_tag", "");
    if (requested_tag.length() == 0) {
        web_server.send(400, "application/json", R"({"status":"error","message":"No release tag was provided."})");
        return;
    }

    String current_version = get_current_firmware_version();
    if (!version_is_newer(requested_tag, current_version)) {
        JsonDocument out;
        out["status"] = "no-update";
        out["currentVersion"] = current_version;
        out["requestedTag"] = requested_tag;
        out["message"] = "The selected release is not newer than the current firmware. Use the rollback selector to install an older tagged release.";
        String payload;
        serializeJson(out, payload);
        web_server.send(200, "application/json", payload);
        return;
    }

    set_saved_release_pref("firmware_pending_tag", requested_tag);

    String error_message;
    bool ok = perform_firmware_ota(requested_tag, error_message);
    JsonDocument out;
    out["status"] = ok ? "updating" : "error";
    out["tag"] = requested_tag;
    out["message"] = ok ? "Starting firmware update for " + requested_tag + ". This may take a minute. The device will reboot when the update completes." : error_message;
    String payload;
    serializeJson(out, payload);
    web_server.send(200, "application/json", payload);

    if (ok) {
        delay(250);
        esp_restart();
    }
}

void handle_firmware_rollback() {
    JsonDocument req;
    String body = web_server.arg("plain");
    if (body.length() > 0) {
        deserializeJson(req, body);
    }

    String tag = req["tag"].is<String>() ? req["tag"].as<String>() : "";
    if (tag.length() == 0) {
        web_server.send(400, "application/json", R"({"status":"error","message":"No rollback tag was provided."})");
        return;
    }

    set_saved_release_pref("firmware_rollback_tag", tag);

    String error_message;
    set_saved_release_pref("firmware_pending_tag", tag);

    bool ok = perform_firmware_ota(tag, error_message);
    JsonDocument out;
    out["status"] = ok ? "rollbacking" : "error";
    out["tag"] = tag;
    out["message"] = ok ? "Rolling back to release " + tag + ". The device will reboot when the previous firmware image is installed." : error_message;
    String payload;
    serializeJson(out, payload);
    web_server.send(200, "application/json", payload);

    if (ok) {
        delay(250);
        esp_restart();
    }
}

void handle_show_firmware_page() {
    String html(reinterpret_cast<const char*>(_binary_web_firmware_html_start));
    web_server.send(200, "text/html", html);
}
void handle_show_signalk_config_page() {
    String html(reinterpret_cast<const char*>(_binary_web_signalk_config_html_start));
    apply_template_value(html, "SIGNALK_HOST", html_escape(get_manual_signalk_host()));
    apply_template_value(html, "SIGNALK_PORT", String(get_manual_signalk_port()));
    apply_template_value(html, "NAVIGATION_RATE_OF_TURN", html_escape(config.navigation_rate_of_turn));
    apply_template_value(html, "NAVIGATION_HEADING_MAGNETIC", html_escape(config.navigation_heading_magnetic));
    apply_template_value(html, "NAVIGATION_POSITION", html_escape(config.navigation_position));
    apply_template_value(html, "NAVIGATION_SPEED_OVER_GROUND", html_escape(config.navigation_speed_over_ground));
    apply_template_value(html, "NAVIGATION_SPEED_THROUGH_WATER", html_escape(config.navigation_speed_through_water));
    apply_template_value(html, "NAVIGATION_COURSE_OVER_GROUND_TRUE", html_escape(config.navigation_course_over_ground_true));
    apply_template_value(html, "NAVIGATION_COURSE_RHUMBLINE_CROSS_TRACK_ERROR", html_escape(config.navigation_course_rhumbline_cross_track_error));
    apply_template_value(html, "NAVIGATION_COURSE_RHUMBLINE_BEARING_TRACK_TRUE", html_escape(config.navigation_course_rhumbline_bearing_track_true));
    apply_template_value(html, "NAVIGATION_COURSE_RHUMBLINE_NEXT_POINT_DISTANCE", html_escape(config.navigation_course_rhumbline_next_point_distance));
    apply_template_value(html, "NAVIGATION_COURSE_RHUMBLINE_NEXT_POINT_VELOCITY_MADE_GOOD", html_escape(config.navigation_course_rhumbline_next_point_velocity_made_good));
    apply_template_value(html, "NAVIGATION_STATE", html_escape(config.navigation_state));
    apply_template_value(html, "NAVIGATION_ATTITUDE_ROLL", html_escape(config.navigation_attitude_roll));
    apply_template_value(html, "NAVIGATION_ATTITUDE_PITCH", html_escape(config.navigation_attitude_pitch));
    apply_template_value(html, "ENVIRONMENT_WIND_ANGLE_APPARENT", html_escape(config.environment_wind_angle_apparent));
    apply_template_value(html, "ENVIRONMENT_WIND_ANGLE_TRUE_GROUND", html_escape(config.environment_wind_angle_true_ground));
    apply_template_value(html, "ENVIRONMENT_WIND_ANGLE_TRUE_WATER", html_escape(config.environment_wind_angle_true_water));
    apply_template_value(html, "ENVIRONMENT_WIND_SPEED_APPARENT", html_escape(config.environment_wind_speed_apparent));
    apply_template_value(html, "ENVIRONMENT_WIND_SPEED_OVER_GROUND", html_escape(config.environment_wind_speed_over_ground));
    apply_template_value(html, "ENVIRONMENT_WIND_SPEED_TRUE", html_escape(config.environment_wind_speed_true));
    apply_template_value(html, "ENVIRONMENT_DEPTH_BELOW_KEEL", html_escape(config.environment_depth_below_keel));
    apply_template_value(html, "ENVIRONMENT_DEPTH_BELOW_TRANSDUCER", html_escape(config.environment_depth_below_transducer));
    apply_template_value(html, "ENVIRONMENT_DEPTH_BELOW_SURFACE", html_escape(config.environment_depth_below_surface));
    apply_template_value(html, "ENVIRONMENT_OUTSIDE_PRESSURE", html_escape(config.environment_outside_pressure));
    apply_template_value(html, "ENVIRONMENT_OUTSIDE_HUMIDITY", html_escape(config.environment_outside_humidity));
    apply_template_value(html, "ENVIRONMENT_OUTSIDE_TEMPERATURE", html_escape(config.environment_outside_temperature));
    apply_template_value(html, "ENVIRONMENT_OUTSIDE_ILLUMINANCE", html_escape(config.environment_outside_illuminance));
    apply_template_value(html, "STEERING_RUDDER_ANGLE", html_escape(config.steering_rudder_angle));
    apply_template_value(html, "VESSEL_DESIGN_BEAM_API", html_escape(config.vessel_design_beam_api));
    apply_template_value(html, "VESSEL_DESIGN_AIR_HEIGHT_API", html_escape(config.vessel_design_air_height_api));
    apply_template_value(html, "VESSEL_DESIGN_DRAFT_API", html_escape(config.vessel_design_draft_api));
    apply_template_value(html, "VESSEL_DESIGN_LENGTH_API", html_escape(config.vessel_design_length_api));
    apply_template_value(html, "VESSEL_NAME_API", html_escape(config.vessel_name_api));
    apply_template_value(html, "VESSEL_MMSI_API", html_escape(config.vessel_mmsi_api));
    apply_template_value(html, "VESSEL_NAVIGATION_STATE_API", html_escape(config.vessel_navigation_state_api));

    String enginePaths;
    for (int i = 0; i < 2; ++i) {
        enginePaths += "<div class='form-group'><label>Engine ";
        enginePaths += String(i);
        enginePaths += " Path:</label><input type='text' name='eng_path_";
        enginePaths += String(i);
        enginePaths += "' value='";
        enginePaths += html_escape(config.engine_paths[i]);
        enginePaths += "'></div>";
    }
    apply_template_value(html, "ENGINE_PATHS", enginePaths);

    String tankPaths;
    for (int i = 0; i < config.num_tanks; ++i) {
        tankPaths += "<div class='form-group'>";
        tankPaths += "<label>Tank ";
        tankPaths += String(i);
        tankPaths += " Path:</label>";
        tankPaths += "<input type='text' name='tank_path_";
        tankPaths += String(i);
        tankPaths += "' placeholder='e.g., tanks.fuel.0.currentLevel' value='";
        tankPaths += html_escape(config.tank_paths[i]);
        tankPaths += "'>";
        tankPaths += "</div>";
    }
    apply_template_value(html, "TANK_PATHS", tankPaths);

    web_server.send(200, "text/html", html);
}
void handle_show_display_config_page() {
    ESP_LOGI("WS", "[DEBUG] handle_show_display_config_page: rendering display config page\n");
    String html(reinterpret_cast<const char*>(_binary_web_display_config_html_start));

    apply_template_value(html, "DIST_METERS_SELECTED", (config.distance_unit == DistanceUnit::Meters) ? "selected" : "");
    apply_template_value(html, "DIST_FEET_SELECTED", (config.distance_unit == DistanceUnit::Feet) ? "selected" : "");
    apply_template_value(html, "TEMP_CELSIUS_SELECTED", (config.temperature_unit == TemperatureUnit::Celsius) ? "selected" : "");
    apply_template_value(html, "TEMP_FAHRENHEIT_SELECTED", (config.temperature_unit == TemperatureUnit::Fahrenheit) ? "selected" : "");
    apply_template_value(html, "NUM_ENGINES", String(config.num_engines));
    apply_template_value(html, "ENG_OIL_ENABLED_CHECKED", config.engine_oil_pressure_enabled ? "checked" : "");
    apply_template_value(html, "ENG_TOP_LEFT_ENABLED_CHECKED", config.engine_top_left_enabled ? "checked" : "");
    apply_template_value(html, "TOP_LEFT_SOG_SELECTED", config.engine_top_left_metric == EngineTopLeftMetric::SOG ? "selected" : "");
    apply_template_value(html, "TOP_LEFT_THROTTLE_SELECTED", config.engine_top_left_metric == EngineTopLeftMetric::ThrottlePercent ? "selected" : "");
    apply_template_value(html, "ENG_TOP_RIGHT_ENABLED_CHECKED", config.engine_top_right_enabled ? "checked" : "");
    apply_template_value(html, "TOP_RIGHT_ALT_SELECTED", config.engine_top_right_metric == EngineTopRightMetric::AlternatorVoltage ? "selected" : "");
    apply_template_value(html, "TOP_RIGHT_BATTERY_SELECTED", config.engine_top_right_metric == EngineTopRightMetric::BatteryVoltage ? "selected" : "");
    apply_template_value(html, "OIL_MIN_STYLE", config.engine_oil_pressure_enabled ? "" : "display:none;");
    apply_template_value(html, "OIL_MAX_STYLE", config.engine_oil_pressure_enabled ? "" : "display:none;");
    apply_template_value(html, "ENGINE_OIL_PRESSURE_MIN", String(config.engine_oil_pressure_min, 1));
    apply_template_value(html, "ENGINE_OIL_PRESSURE_MAX", String(config.engine_oil_pressure_max, 1));
    apply_template_value(html, "ENGINE_TEMP_REDLINE", String(config.engine_temp_redline, 1));
    apply_template_value(html, "NUM_TANKS", String(config.num_tanks));
    apply_template_value(html, "DEPTH_CHART_DURATION", String(config.depth_chart_duration));
    apply_template_value(html, "SPEED_CHART_DURATION", String(config.speed_chart_duration));

    String engineScreens;
    for (int i = 0; i < config.num_engines; ++i) {
        char id[20];
        sprintf(id, "engine_%d", i);
        engineScreens += "<div><input type='checkbox' name='screen_";
        engineScreens += id;
        engineScreens += "' ";
        if (is_screen_enabled(id)) {
            engineScreens += "checked";
        }
        engineScreens += "> Engine ";
        engineScreens += String(i + 1);
        engineScreens += "</div>";
    }
    apply_template_value(html, "ENGINE_SCREENS", engineScreens);

    String staticScreens;
    const char* static_screens[] = {"wind", "depth", "speed", "compass", "gps", "tanks", "heel"};
    for (int i = 0; i < 7; ++i) {
        const char* id = static_screens[i];
        staticScreens += "<div><input type='checkbox' name='screen_";
        staticScreens += id;
        staticScreens += "' ";
        if (is_screen_enabled(id)) {
            staticScreens += "checked";
        }
        staticScreens += "> ";
        staticScreens += id;
        staticScreens += "</div>";
    }
    apply_template_value(html, "STATIC_SCREENS", staticScreens);

    web_server.send(200, "text/html", html);
}
void handle_save_config() {
    // Read JSON body from POST request
    // Get content length from server parameter
    String body = web_server.arg("plain");
    
    if (body.length() == 0) {
        web_server.send(400, "text/plain", "Empty request body");
        return;
    }
    
    // Parse JSON
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        ESP_LOGE("HTTP", "JSON parse error: %s", error.c_str());
        web_server.send(400, "text/plain", "Invalid JSON");
        return;
    }
    
    if (!doc["signalk_override_host"].isNull()) {
        String host = doc["signalk_override_host"].as<String>();
        host.trim();
        Preferences netPrefs;
        netPrefs.begin("signalk", false);
        if (host.length() > 0) {
            netPrefs.putString(SK_MANUAL_HOST_PREF, host);
            netPrefs.putString(SK_TCP_HOST_PREF, host);
            netPrefs.putString(SK_HTTP_HOST_PREF, host);
        } else {
            netPrefs.remove(SK_MANUAL_HOST_PREF);
            netPrefs.remove(SK_TCP_HOST_PREF);
            netPrefs.remove(SK_HTTP_HOST_PREF);
        }
        netPrefs.end();
    }
    if (!doc["signalk_override_port"].isNull()) {
        int port = doc["signalk_override_port"].as<int>();
        if (port < 1) port = 3000;
        if (port > 65535) port = 65535;
        Preferences netPrefs;
        netPrefs.begin("signalk", false);
        netPrefs.putInt(SK_MANUAL_PORT_PREF, port);
        netPrefs.putInt(SK_TCP_PORT_PREF, port);
        netPrefs.putInt(SK_HTTP_PORT_PREF, port);
        netPrefs.end();
    }

    // Load all string fields (Signal K paths only)
    if (!doc["nav_rot"].isNull()) config.navigation_rate_of_turn = doc["nav_rot"].as<String>();
    if (!doc["nav_hdg"].isNull()) config.navigation_heading_magnetic = doc["nav_hdg"].as<String>();
    if (!doc["nav_pos"].isNull()) config.navigation_position = doc["nav_pos"].as<String>();
    if (!doc["nav_sog"].isNull()) config.navigation_speed_over_ground = doc["nav_sog"].as<String>();
    if (!doc["nav_stw"].isNull()) config.navigation_speed_through_water = doc["nav_stw"].as<String>();
    if (!doc["nav_cog"].isNull()) config.navigation_course_over_ground_true = doc["nav_cog"].as<String>();
    if (!doc["nav_xte"].isNull()) config.navigation_course_rhumbline_cross_track_error = doc["nav_xte"].as<String>();
    if (!doc["nav_brg"].isNull()) config.navigation_course_rhumbline_bearing_track_true = doc["nav_brg"].as<String>();
    if (!doc["nav_dist"].isNull()) config.navigation_course_rhumbline_next_point_distance = doc["nav_dist"].as<String>();
    if (!doc["nav_vmg"].isNull()) config.navigation_course_rhumbline_next_point_velocity_made_good = doc["nav_vmg"].as<String>();
    if (!doc["nav_state"].isNull()) config.navigation_state = doc["nav_state"].as<String>();
    if (!doc["nav_roll"].isNull()) config.navigation_attitude_roll = doc["nav_roll"].as<String>();
    if (!doc["nav_pitch"].isNull()) config.navigation_attitude_pitch = doc["nav_pitch"].as<String>();

    if (!doc["env_waa"].isNull()) config.environment_wind_angle_apparent = doc["env_waa"].as<String>();
    if (!doc["env_watg"].isNull()) config.environment_wind_angle_true_ground = doc["env_watg"].as<String>();
    if (!doc["env_watw"].isNull()) config.environment_wind_angle_true_water = doc["env_watw"].as<String>();
    if (!doc["env_wsa"].isNull()) config.environment_wind_speed_apparent = doc["env_wsa"].as<String>();
    if (!doc["env_wsog"].isNull()) config.environment_wind_speed_over_ground = doc["env_wsog"].as<String>();
    if (!doc["env_wst"].isNull()) config.environment_wind_speed_true = doc["env_wst"].as<String>();
    if (!doc["env_dbk"].isNull()) config.environment_depth_below_keel = doc["env_dbk"].as<String>();
    if (!doc["env_dbt"].isNull()) config.environment_depth_below_transducer = doc["env_dbt"].as<String>();
    if (!doc["env_dbs"].isNull()) config.environment_depth_below_surface = doc["env_dbs"].as<String>();
    if (!doc["env_press"].isNull()) config.environment_outside_pressure = doc["env_press"].as<String>();
    if (!doc["env_humid"].isNull()) config.environment_outside_humidity = doc["env_humid"].as<String>();
    if (!doc["env_temp"].isNull()) config.environment_outside_temperature = doc["env_temp"].as<String>();
    if (!doc["env_illum"].isNull()) config.environment_outside_illuminance = doc["env_illum"].as<String>();

    if (!doc["steer_rudder"].isNull()) config.steering_rudder_angle = doc["steer_rudder"].as<String>();

    if (!doc["vessel_beam"].isNull()) config.vessel_design_beam_api = doc["vessel_beam"].as<String>();
    if (!doc["vessel_air"].isNull()) config.vessel_design_air_height_api = doc["vessel_air"].as<String>();
    if (!doc["vessel_draft"].isNull()) config.vessel_design_draft_api = doc["vessel_draft"].as<String>();
    if (!doc["vessel_len"].isNull()) config.vessel_design_length_api = doc["vessel_len"].as<String>();
    if (!doc["vessel_name"].isNull()) config.vessel_name_api = doc["vessel_name"].as<String>();
    if (!doc["vessel_mmsi"].isNull()) config.vessel_mmsi_api = doc["vessel_mmsi"].as<String>();
    if (!doc["vessel_nav"].isNull()) config.vessel_navigation_state_api = doc["vessel_nav"].as<String>();

    // Load engine paths (up to 8 engines)
    for (int i = 0; i < 8; i++) {
        String key = String("eng_path_") + i;
        if (!doc[key].isNull()) {
            config.engine_paths[i] = doc[key].as<String>();
        }
    }

    // Load tank paths (up to 8 tanks)
    for (int i = 0; i < 8; i++) {
        String key = String("tank_path_") + i;
        if (!doc[key].isNull()) {
            config.tank_paths[i] = doc[key].as<String>();
        }
    }

    // Save all to preferences
    save_all_config_to_preferences();

    web_server.send(200, "text/plain", "Configuration saved");
}

void handle_save_display_config() {
    // Read JSON body from POST request
    String body = web_server.arg("plain");
    ESP_LOGI("HTTP", "Body length: %d", body.length());
    ESP_LOGI("HTTP", "Body: %s", body.c_str());
    
    if (body.length() == 0) {
        web_server.send(400, "text/plain", "Empty request body");
        return;
    }
    
    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        ESP_LOGE("HTTP", "JSON parse error: %s", error.c_str());
        web_server.send(400, "text/plain", "Invalid JSON: " + String(error.c_str()));
        return;
    }

    // Load display configuration fields
    if (doc["dist_unit"].is<int>()) {
        config.distance_unit = (doc["dist_unit"].as<int>() == 0) ? DistanceUnit::Meters : DistanceUnit::Feet;
    }
    if (doc["temp_unit"].is<int>()) {
        config.temperature_unit = (doc["temp_unit"].as<int>() == 0) ? TemperatureUnit::Celsius : TemperatureUnit::Fahrenheit;
    }
    if (!doc["num_engines"].isNull()) {
        int new_val = doc["num_engines"].as<int>();
        config.num_engines = new_val;
        if (config.num_engines > 8) config.num_engines = 8;
        if (config.num_engines < 1) config.num_engines = 1;
    }
    if (!doc["eng_oil_enabled"].isNull()) config.engine_oil_pressure_enabled = doc["eng_oil_enabled"].as<bool>();
    if (!doc["eng_top_left_enabled"].isNull()) config.engine_top_left_enabled = doc["eng_top_left_enabled"].as<bool>();
    if (!doc["eng_top_left_metric"].isNull()) {
        int metric = doc["eng_top_left_metric"].as<int>();
        if (metric == (int)EngineTopLeftMetric::SOG || metric == (int)EngineTopLeftMetric::ThrottlePercent) {
            config.engine_top_left_metric = static_cast<EngineTopLeftMetric>(metric);
        }
    }
    if (!doc["eng_top_right_enabled"].isNull()) config.engine_top_right_enabled = doc["eng_top_right_enabled"].as<bool>();
    if (!doc["eng_top_right_metric"].isNull()) {
        int metric = doc["eng_top_right_metric"].as<int>();
        if (metric == (int)EngineTopRightMetric::AlternatorVoltage || metric == (int)EngineTopRightMetric::BatteryVoltage) {
            config.engine_top_right_metric = static_cast<EngineTopRightMetric>(metric);
        }
    }
    if (!doc["eng_oil_min"].isNull())  config.engine_oil_pressure_min = doc["eng_oil_min"].as<float>();
    if (!doc["eng_oil_max"].isNull()) config.engine_oil_pressure_max = doc["eng_oil_max"].as<float>();
    if (!doc["eng_temp_red"].isNull()) config.engine_temp_redline = doc["eng_temp_red"].as<float>();
    if (!doc["num_tanks"].isNull()) {
        config.num_tanks = doc["num_tanks"].as<int>();
        if (config.num_tanks > 8) config.num_tanks = 8;
        if (config.num_tanks < 1) config.num_tanks = 1;
    }
    if (!doc["depth_chart_min"].isNull()) config.depth_chart_duration = doc["depth_chart_min"].as<int>();
    if (!doc["speed_chart_min"].isNull()) config.speed_chart_duration = doc["speed_chart_min"].as<int>();

    // Handle screen toggles
    for (int i = 0; i < config.num_engines; i++) {
        char id[20];
        sprintf(id, "engine_%d", i);

        String param = "screen_" + String(id);

        if (!doc[param].isNull()) {
            bool enabled = doc[param].as<bool>();
            set_screen_enabled(id, enabled);
        }
    }

    // Static screens
    const char* static_screens[] = {
        "wind", "depth", "speed", "compass", "gps", "tanks", "heel"
    };

    for (int i = 0; i < 7; i++) {
        String param = "screen_" + String(static_screens[i]);
        if (!doc[param].isNull()) {
            bool enabled = doc[param].as<bool>();
            set_screen_enabled(static_screens[i], enabled);
        }
    }

    // Save all to preferences and defer the LVGL re-init until the main loop is safe.
    save_all_config_to_preferences();
    notify_config_changed();

    web_server.send(200, "text/plain", "Display settings saved");
}

void handle_export_config() {
    JsonDocument doc;
    export_config_to_json(doc);
    String json;
    serializeJson(doc, json);
    web_server.send(200, "application/json", json);
}

void handle_reset_config() {
    Preferences prefs;
    prefs.begin(kPrefsNamespace, false);
    prefs.clear();
    prefs.end();
    load_config_from_preferences();
    web_server.send(200, "text/plain", "Configuration reset to defaults");
}

signalk_path_config_t& get_signalk_path_config() {
    return config;
}

static void check_release_status_on_boot() {
    std::vector<String> tags;
    std::vector<String> download_urls;
    String error_message;
    if (!fetch_release_history(tags, download_urls, error_message)) {
        return;
    }

    if (tags.empty()) {
        return;
    }

    String latest_tag = tags[0];
    String latest_downloader = download_urls[0];
    set_saved_release_pref("firmware_latest_tag", latest_tag);
    set_saved_release_pref("firmware_latest_url", latest_downloader);
}

void load_signalk_path_config() {
    load_config_from_preferences();
    check_release_status_on_boot();
}

void signalk_path_config_web_begin() {
    if (web_server_started) {
        return;
    }

    load_config_from_preferences();
    // Admin index page
    web_server.on("/", HTTP_GET, handle_show_admin_index);
    web_server.on("/firmware", HTTP_GET, handle_show_firmware_page);
    web_server.on("/firmware/status", HTTP_GET, handle_firmware_status);
    web_server.on("/firmware/skip", HTTP_POST, handle_firmware_skip);
    web_server.on("/firmware/update", HTTP_POST, handle_firmware_update);
    web_server.on("/firmware/rollback", HTTP_POST, handle_firmware_rollback);
    
    // Signal K path configuration
    web_server.on("/signalk-config", HTTP_GET, handle_show_signalk_config_page);
    web_server.on("/signalk-config/save", HTTP_POST, handle_save_config);
    web_server.on("/signalk-config/export", HTTP_GET, handle_export_config);
    web_server.on("/signalk-config/reset", HTTP_POST, handle_reset_config);
    
    // Display configuration
    web_server.on("/display-config", HTTP_GET, handle_show_display_config_page);
    web_server.on("/display-config/save", HTTP_POST, handle_save_display_config);
    web_server.begin();
    web_server_started = true;
}

void signalk_path_config_web_loop() {
    if (web_server_started) {
        web_server.handleClient();
    }
}
