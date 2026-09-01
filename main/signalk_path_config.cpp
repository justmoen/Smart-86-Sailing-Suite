#include "signalk_path_config.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <vector>
#include <esp_https_ota.h>
#include <esp_crt_bundle.h>
#include <esp_system.h>
#include "net_mdns.h"
#include "net_signalk_http.h"
#include "screen_config.h"
#include "ui_manager.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "v0.6.0"
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
static String get_saved_release_pref(const char* key, const String& fallback);
static void set_saved_release_pref(const char* key, const String& value);
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

static bool fetch_release_history(std::vector<String>& tags, std::vector<String>& download_urls, String& error_message) {
    tags.clear();
    download_urls.clear();

    String payload = http_get_text(String("https://api.github.com/repos/") + kGitHubRepo + "/releases");
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

    JsonArray arr = doc.as<JsonArray>();
    for (JsonVariant release : arr) {
        if (!release["tag_name"].is<String>()) {
            continue;
        }

        String tag = release["tag_name"].as<String>();
        if (tag.length() == 0) {
            continue;
        }

        String asset_url = "";
        if (release["html_url"].is<String>()) {
            asset_url = release["html_url"].as<String>();
        }

        if (release["assets"].is<JsonArray>()) {
            JsonArray assets = release["assets"].as<JsonArray>();
            for (JsonVariant asset : assets) {
                if (asset["browser_download_url"].is<String>()) {
                    asset_url = asset["browser_download_url"].as<String>();
                    break;
                }
            }
        }

        tags.push_back(tag);
        download_urls.push_back(asset_url);
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

static String resolve_release_asset_url(const String& tag, const String& fallback_download_url = "") {
    String candidate = fallback_download_url;
    if (candidate.length() == 0) {
        candidate = get_saved_release_pref("firmware_latest_url", "");
    }

    if (tag.length() == 0) {
        return candidate;
    }

    String api_url = String("https://api.github.com/repos/") + kGitHubRepo + "/releases/tags/" + tag;
    String payload = http_get_text(api_url);
    if (payload.length() == 0) {
        return candidate;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        return candidate;
    }

    if (doc["html_url"].is<String>()) {
        candidate = doc["html_url"].as<String>();
    }

    if (doc["assets"].is<JsonArray>()) {
        JsonArray assets = doc["assets"].as<JsonArray>();
        for (JsonVariant asset : assets) {
            if (asset["browser_download_url"].is<String>()) {
                candidate = asset["browser_download_url"].as<String>();
                break;
            }
        }
    }

    return candidate;
}

static String get_saved_release_pref(const char* key, const String& fallback) {
    return load_string_pref(key, fallback.c_str());
}

static void set_saved_release_pref(const char* key, const String& value) {
    save_string_pref(key, value);
}

static bool is_release_skipped(const String& tag) {
    if (tag.length() == 0) {
        return false;
    }

    String skipped_tag = get_saved_release_pref("firmware_skipped_tag", "");
    return skipped_tag.length() > 0 && skipped_tag.equalsIgnoreCase(tag);
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

static bool perform_firmware_ota(const String& tag, String& error_message) {
    String download_url = resolve_release_asset_url(tag, get_saved_release_pref("firmware_latest_url", ""));
    if (download_url.length() == 0) {
        error_message = "No downloadable firmware asset was found for release " + tag + ".";
        return false;
    }

    if (download_url.indexOf("http") != 0) {
        error_message = "The firmware URL for release " + tag + " is not valid.";
        return false;
    }

    esp_http_client_config_t client_config = {};
    client_config.url = download_url.c_str();
    client_config.timeout_ms = 30000;
    client_config.keep_alive_enable = true;
    client_config.buffer_size = 4096;
    client_config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &client_config;
    ota_config.bulk_flash_erase = false;
    ota_config.partial_http_download = true;
    ota_config.max_http_request_size = 65536;

    esp_err_t err = esp_https_ota(&ota_config);
    if (err != ESP_OK) {
        error_message = "OTA failed for " + tag + ": " + String(esp_err_to_name(err));
        return false;
    }

    error_message = "Firmware update staged successfully. Rebooting now...";
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
    Serial.printf("[DEBUG] config.engine_top_left_enabled loaded: value=%d, type=bool\n", config.engine_top_left_enabled ? 1 : 0);
    int top_left_metric = load_int_pref_legacy("eng_tl_met", "eng_top_left_metric", (int)EngineTopLeftMetric::SOG);
    if (top_left_metric < 0 || top_left_metric > 1) top_left_metric = (int)EngineTopLeftMetric::SOG;
    config.engine_top_left_metric = static_cast<EngineTopLeftMetric>(top_left_metric);
    Serial.printf("[DEBUG] config.engine_top_left_metric loaded: raw=%d, enum=%d, type=int\n", top_left_metric, static_cast<int>(config.engine_top_left_metric));
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
    String html = R"(<!DOCTYPE html>
<html>
<head>
<meta charset='utf-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Configuration Administration</title>
<style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%);
        min-height: 100vh;
        display: flex;
        align-items: center;
        justify-content: center;
        padding: 20px;
    }
    .container {
        background: white;
        border-radius: 12px;
        box-shadow: 0 20px 60px rgba(0,0,0,0.3);
        overflow: hidden;
        max-width: 600px;
        width: 100%;
    }
    .header {
        background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%);
        color: white;
        padding: 40px 30px;
        text-align: center;
    }
    .header h1 { font-size: 2em; margin-bottom: 10px; }
    .header p { opacity: 0.9; }
    .content {
        padding: 40px 30px;
    }
    .menu-item {
        display: block;
        padding: 20px;
        margin: 15px 0;
        background: #f5f5f5;
        border: 2px solid #e0e0e0;
        border-radius: 8px;
        text-decoration: none;
        color: #333;
        font-size: 1.1em;
        font-weight: 600;
        transition: all 0.2s;
        cursor: pointer;
        text-align: center;
    }
    .menu-item:hover {
        background: #e8f4f8;
        border-color: #2a5298;
        transform: translateY(-2px);
        box-shadow: 0 4px 12px rgba(42, 82, 152, 0.2);
    }
    .menu-item.signalk { border-left: 4px solid #2196F3; }
    .menu-item.display { border-left: 4px solid #4CAF50; }
    .menu-item.firmware { border-left: 4px solid #ff9800; }
    .description {
        font-size: 0.85em;
        color: #666;
        margin-top: 5px;
        font-weight: normal;
    }
</style>
</head>
<body>
<div class='container'>
    <div class='header'>
        <h1>⚙️ Configuration</h1>
        <p>Select an option to configure</p>
    </div>
    <div class='content'>
        <a href='/signalk-config' class='menu-item signalk'>
            Signal K Path Configuration
            <div class='description'>Configure data paths from your Signal K server</div>
        </a>
        <a href='/display-config' class='menu-item display'>
            Display Settings
            <div class='description'>Customize gauges, units, and chart history</div>
        </a>
        <a href='/firmware' class='menu-item firmware'>
            Firmware Updates
            <div class='description'>Check GitHub releases, update, and rollback to older versions</div>
        </a>
    </div>
</div>
</body>
</html>
)";
    web_server.send(200, "text/html", html);
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
    String html;
    html.reserve(20000);
    html += R"(<!DOCTYPE html>
<html>
<head>
<meta charset='utf-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Firmware Updates</title>
<style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%);
        color: #333;
        min-height: 100vh;
        padding: 20px;
    }
    .container {
        max-width: 900px;
        margin: 0 auto;
        background: white;
        border-radius: 12px;
        box-shadow: 0 20px 60px rgba(0,0,0,0.3);
        overflow: hidden;
    }
    .header {
        background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%);
        color: white;
        padding: 30px;
        text-align: center;
    }
    .header h1 { font-size: 2em; margin-bottom: 10px; }
    .content { padding: 30px; }
    .section {
        margin: 25px 0;
        padding: 20px;
        background: #f5f5f5;
        border-radius: 8px;
        border-left: 4px solid #ff9800;
    }
    .section h2 { margin-bottom: 12px; font-size: 1.2em; }
    .row { display: flex; gap: 12px; align-items: center; flex-wrap: wrap; margin-top: 12px; }
    button, select {
        padding: 12px 18px;
        border-radius: 6px;
        border: 1px solid #ddd;
        font-size: 1em;
    }
    button {
        background: #ff9800;
        color: white;
        border: none;
        font-weight: 600;
        cursor: pointer;
    }
    button.secondary {
        background: #2196F3;
    }
    button.danger {
        background: #e53935;
    }
    .status {
        background: #fff8e1;
        border-left: 4px solid #ffb300;
        padding: 12px 14px;
        border-radius: 6px;
        margin-top: 12px;
        color: #5d4700;
    }
    .meta {
        font-size: 0.9em;
        color: #666;
        margin-top: 8px;
    }
    .back-link {
        display: inline-block;
        margin-bottom: 20px;
        padding: 8px 16px;
        background: #f5f5f5;
        border-radius: 6px;
        text-decoration: none;
        color: #333;
        font-weight: 600;
        border: 1px solid #ddd;
    }
    select {
        min-width: 220px;
        background: white;
    }
</style>
</head>
<body>
<div class='container'>
    <div class='header'>
        <h1>Firmware Updates</h1>
        <p>Check for software releases and choose a rollback target</p>
    </div>
    <div class='content'>
        <a class='back-link' href='/'>← Back to Administration</a>

        <div class='section'>
            <h2>Current Firmware</h2>
            <div class='meta'>Current version: <strong id='currentVersion'>loading...</strong></div>
            <div class='meta'>Latest release found: <strong id='latestVersion'>checking...</strong></div>
            <div id='statusBox' class='status'>Checking GitHub for the most recent release...</div>
            <div class='row'>
                <button class='secondary' id='checkBtn' type='button'>Check for Updates</button>
                <button id='updateBtn' type='button'>Update to Latest Release</button>
                <button class='secondary' id='skipBtn' type='button'>Skip This Release</button>
            </div>
        </div>

        <div class='section'>
            <h2>Rollback to a Recent Release</h2>
            <div class='row'>
                <select id='rollbackSelect'>
                    <option value=''>Loading recent releases...</option>
                </select>
                <button class='danger' id='rollbackBtn' type='button'>Install Selected Rollback</button>
            </div>
            <div class='meta'>This keeps the last five public GitHub release tags available for user-selected rollback and will flash the chosen release only when you confirm.</div>
        </div>
    </div>
</div>
<script>
const currentVersionEl = document.getElementById('currentVersion');
const latestVersionEl = document.getElementById('latestVersion');
const statusBoxEl = document.getElementById('statusBox');
const rollbackSelectEl = document.getElementById('rollbackSelect');

async function loadStatus() {
    try {
        const res = await fetch('/firmware/status');
        const data = await res.json();
        currentVersionEl.textContent = data.currentVersion || 'unknown';
        latestVersionEl.textContent = data.latestVersion || 'unknown';
        statusBoxEl.textContent = data.message || 'No status available.';

        const options = [];
        if (Array.isArray(data.releases)) {
            data.releases.forEach((release) => {
                if (release && release.tag) {
                    options.push(`<option value='${release.tag}'>${release.tag}</option>`);
                }
            });
        }

        if (options.length > 0) {
            rollbackSelectEl.innerHTML = '<option value="">Select a prior release</option>' + options.join('');
        } else {
            rollbackSelectEl.innerHTML = '<option value="">No releases available</option>';
        }

        const updateBtn = document.getElementById('updateBtn');
        const skipBtn = document.getElementById('skipBtn');
        if (data.hasUpdate) {
            updateBtn.disabled = false;
            updateBtn.textContent = `Update to ${data.latestVersion}`;
            skipBtn.disabled = false;
            skipBtn.textContent = `Skip ${data.latestVersion}`;
        } else if (data.skipThisRelease) {
            updateBtn.disabled = true;
            updateBtn.textContent = 'Skipped release';
            skipBtn.disabled = true;
            skipBtn.textContent = 'Release skipped';
        } else {
            updateBtn.disabled = true;
            updateBtn.textContent = 'No update available';
            skipBtn.disabled = true;
            skipBtn.textContent = 'Skip This Release';
        }
    } catch (err) {
        statusBoxEl.textContent = 'Error contacting the firmware update endpoint.';
        console.error(err);
    }
}

document.getElementById('checkBtn').addEventListener('click', loadStatus);

document.getElementById('updateBtn').addEventListener('click', async () => {
    const latestTag = document.getElementById('latestVersion').textContent.trim();
    if (!latestTag || latestTag === 'unknown' || latestTag === 'checking...') {
        statusBoxEl.textContent = 'There is no newer release to install yet.';
        return;
    }

    statusBoxEl.textContent = 'Requesting update confirmation for ' + latestTag + '...';
    const res = await fetch('/firmware/update', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ tag: latestTag })
    });
    const data = await res.json();
    statusBoxEl.textContent = data.message || 'Update request sent.';
    await loadStatus();
});

document.getElementById('skipBtn').addEventListener('click', async () => {
    const latestTag = document.getElementById('latestVersion').textContent.trim();
    if (!latestTag || latestTag === 'unknown' || latestTag === 'checking...') {
        statusBoxEl.textContent = 'There is no release to skip yet.';
        return;
    }

    statusBoxEl.textContent = 'Skipping release ' + latestTag + '...';
    const res = await fetch('/firmware/skip', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ tag: latestTag })
    });
    const data = await res.json();
    statusBoxEl.textContent = data.message || 'Release skipped.';
    await loadStatus();
});

document.getElementById('rollbackBtn').addEventListener('click', async () => {
    const tag = rollbackSelectEl.value;
    if (!tag) {
        statusBoxEl.textContent = 'Choose a release from the list before saving the rollback target.';
        return;
    }

    const res = await fetch('/firmware/rollback', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ tag })
    });
    const data = await res.json();
    statusBoxEl.textContent = data.message || 'Rollback target saved.';
});

loadStatus();
</script>
</body>
</html>
)";
    web_server.send(200, "text/html", html);
}

void handle_show_signalk_config_page() {
    String html;
    html.reserve(16000);
    html += R"(<!DOCTYPE html>
<html>
<head>
<meta charset='utf-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Signal K Path Configuration</title>
<style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%);
        color: #333;
        min-height: 100vh;
        padding: 20px;
    }
    .container {
        max-width: 1000px;
        margin: 0 auto;
        background: white;
        border-radius: 12px;
        box-shadow: 0 20px 60px rgba(0,0,0,0.3);
        overflow: hidden;
    }
    .header {
        background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%);
        color: white;
        padding: 30px;
        text-align: center;
    }
    .header h1 { font-size: 2em; margin-bottom: 10px; }
    .header p { opacity: 0.9; font-size: 0.95em; }
    .content { padding: 30px; }
    .tree-item {
        margin: 10px 0;
        border: 1px solid #e0e0e0;
        border-radius: 8px;
        overflow: hidden;
    }
    .tree-toggle {
        display: flex;
        align-items: center;
        padding: 15px;
        background: #f5f5f5;
        cursor: pointer;
        user-select: none;
        border: none;
        width: 100%;
        text-align: left;
        font-size: 1em;
        font-weight: 600;
        color: #333;
        transition: background 0.2s;
    }
    .tree-toggle:hover { background: #efefef; }
    .tree-toggle.expanded { background: #e8f4f8; }
    .tree-toggle::before {
        content: '▶';
        display: inline-block;
        margin-right: 10px;
        transition: transform 0.2s;
    }
    .tree-toggle.expanded::before { transform: rotate(90deg); }
    .tree-content {
        display: none;
        padding: 20px;
        background: white;
        border-top: 1px solid #e0e0e0;
    }
    .tree-content.expanded { display: block; }
    .nested-toggle {
        display: flex;
        align-items: center;
        padding: 12px;
        background: #fafafa;
        cursor: pointer;
        user-select: none;
        border: none;
        width: 100%;
        text-align: left;
        font-size: 0.95em;
        font-weight: 600;
        color: #555;
        transition: background 0.2s;
        border-left: 3px solid #ddd;
        margin: 15px 0 10px 0;
    }
    .nested-toggle:hover { background: #f0f0f0; }
    .nested-toggle.expanded { background: #e8f4f8; }
    .nested-toggle::before {
        content: '▶';
        display: inline-block;
        margin-right: 8px;
        transition: transform 0.2s;
        font-size: 0.8em;
    }
    .nested-toggle.expanded::before { transform: rotate(90deg); }
    .nested-content {
        display: none;
        padding: 15px;
        margin-left: 15px;
        background: #fafafa;
        border-left: 2px solid #ddd;
    }
    .nested-content.expanded { display: block; }
    .form-group {
        margin-bottom: 15px;
    }
    label {
        display: block;
        font-weight: 600;
        margin-bottom: 5px;
        color: #333;
        font-size: 0.95em;
    }
    input[type="text"],
    input[type="number"] {
        width: 100%;
        padding: 10px;
        border: 1px solid #ddd;
        border-radius: 6px;
        font-size: 0.95em;
        font-family: 'Monaco', 'Courier New', monospace;
    }
    input:focus {
        outline: none;
        border-color: #2a5298;
        box-shadow: 0 0 0 3px rgba(42, 82, 152, 0.1);
    }
    .actions {
        display: flex;
        gap: 12px;
        margin-top: 30px;
        flex-wrap: wrap;
    }
    button {
        padding: 12px 24px;
        border: none;
        border-radius: 6px;
        font-size: 1em;
        font-weight: 600;
        cursor: pointer;
        transition: all 0.2s;
    }
    .btn-save {
        background: #4CAF50;
        color: white;
    }
    .btn-save:hover { background: #45a049; transform: translateY(-2px); box-shadow: 0 4px 12px rgba(76, 175, 80, 0.3); }
    .btn-export {
        background: #2196F3;
        color: white;
    }
    .btn-export:hover { background: #0b7dda; transform: translateY(-2px); box-shadow: 0 4px 12px rgba(33, 150, 243, 0.3); }
    .btn-reset {
        background: #f44336;
        color: white;
    }
    .btn-reset:hover { background: #d32f2f; transform: translateY(-2px); box-shadow: 0 4px 12px rgba(244, 67, 54, 0.3); }
    .info-text {
        background: #e3f2fd;
        border-left: 4px solid #2196F3;
        padding: 15px;
        margin: 20px 0;
        border-radius: 4px;
        color: #1565c0;
        font-size: 0.95em;
    }
    .info-text {
        background: #e3f2fd;
        border-left: 4px solid #2196F3;
        padding: 15px;
        margin: 20px 0;
        border-radius: 4px;
        color: #1565c0;
        font-size: 0.95em;
    }
    .back-link {
        display: inline-block;
        margin-bottom: 20px;
        padding: 8px 16px;
        background: #f5f5f5;
        border-radius: 6px;
        text-decoration: none;
        color: #333;
        font-weight: 600;
        border: 1px solid #ddd;
    }
    .back-link:hover {
        background: #efefef;
    }
</style>
</head>
<body>
<div class='container'>
    <div class='header'>
        <h1>Signal K Configuration</h1>
        <p>Manage Signal K data paths and display thresholds</p>
    </div>
    <div class='content'>
        <div class='info-text'>
            Configure Signal K paths for your boat's data model. Sections can be expanded to view and edit paths.
        </div>
        <div class='tree-item'>
            <button type='button' class='tree-toggle expanded' onclick='toggleTree(this)'>Signal K Connection</button>
            <div class='tree-content expanded'>
                <div class='info-text'>
                    If mDNS is unavailable on your network, set a fixed IP address and port here for the Signal K server.
                </div>)";
    html += "<div class='form-group'><label>Signal K Server IP / Host:</label><input type='text' name='signalk_override_host' value='";
    html += get_manual_signalk_host();
    html += "'></div>";
    html += "<div class='form-group'><label>Signal K Server Port:</label><input type='number' name='signalk_override_port' value='";
    html += String(get_manual_signalk_port());
    html += R"(' min='1' max='65535'></div>
            </div>
        </div>
        <form id='configForm'>
)";

    // Navigation section
    html += R"(
            <div class='tree-item'>
                <button type='button' class='tree-toggle expanded' onclick='toggleTree(this)'>Navigation</button>
                <div class='tree-content expanded'>
)";
    html += "<div class='form-group'><label>Rate of Turn:</label><input type='text' name='nav_rot' value='" + config.navigation_rate_of_turn + "'></div>";
    html += "<div class='form-group'><label>Heading Magnetic:</label><input type='text' name='nav_hdg' value='" + config.navigation_heading_magnetic + "'></div>";
    html += "<div class='form-group'><label>Position:</label><input type='text' name='nav_pos' value='" + config.navigation_position + "'></div>";
    html += "<div class='form-group'><label>Speed Over Ground:</label><input type='text' name='nav_sog' value='" + config.navigation_speed_over_ground + "'></div>";
    html += "<div class='form-group'><label>Speed Through Water:</label><input type='text' name='nav_stw' value='" + config.navigation_speed_through_water + "'></div>";
    html += "<div class='form-group'><label>Course Over Ground True:</label><input type='text' name='nav_cog' value='" + config.navigation_course_over_ground_true + "'></div>";
    html += "<button type='button' class='nested-toggle expanded' onclick='toggleNested(event)'>Rhumbline Course</button>";
    html += "<div class='nested-content expanded'>";
    html += "<div class='form-group'><label>Cross Track Error:</label><input type='text' name='nav_xte' value='" + config.navigation_course_rhumbline_cross_track_error + "'></div>";
    html += "<div class='form-group'><label>Bearing Track True:</label><input type='text' name='nav_brg' value='" + config.navigation_course_rhumbline_bearing_track_true + "'></div>";
    html += "<div class='form-group'><label>Next Point Distance:</label><input type='text' name='nav_dist' value='" + config.navigation_course_rhumbline_next_point_distance + "'></div>";
    html += "<div class='form-group'><label>Next Point VMG:</label><input type='text' name='nav_vmg' value='" + config.navigation_course_rhumbline_next_point_velocity_made_good + "'></div>";
    html += "</div>";
    html += "<div class='form-group'><label>Navigation State:</label><input type='text' name='nav_state' value='" + config.navigation_state + "'></div>";
    html += "<button type='button' class='nested-toggle expanded' onclick='toggleNested(event)'>Attitude</button>";
    html += "<div class='nested-content expanded'>";
    html += "<div class='form-group'><label>Roll:</label><input type='text' name='nav_roll' value='" + config.navigation_attitude_roll + "'></div>";
    html += "<div class='form-group'><label>Pitch:</label><input type='text' name='nav_pitch' value='" + config.navigation_attitude_pitch + "'></div>";
    html += "</div>";
    html += "                </div></div>";

    // Environment section
    html += R"(
            <div class='tree-item'>
                <button type='button' class='tree-toggle expanded' onclick='toggleTree(this)'>Environment</button>
                <div class='tree-content expanded'>
)";
    html += "<button type='button' class='nested-toggle expanded' onclick='toggleNested(event)'>Wind</button>";
    html += "<div class='nested-content expanded'>";
    html += "<div class='form-group'><label>Angle Apparent:</label><input type='text' name='env_waa' value='" + config.environment_wind_angle_apparent + "'></div>";
    html += "<div class='form-group'><label>Angle True Ground:</label><input type='text' name='env_watg' value='" + config.environment_wind_angle_true_ground + "'></div>";
    html += "<div class='form-group'><label>Angle True Water:</label><input type='text' name='env_watw' value='" + config.environment_wind_angle_true_water + "'></div>";
    html += "<div class='form-group'><label>Speed Apparent:</label><input type='text' name='env_wsa' value='" + config.environment_wind_speed_apparent + "'></div>";
    html += "<div class='form-group'><label>Speed Over Ground:</label><input type='text' name='env_wsog' value='" + config.environment_wind_speed_over_ground + "'></div>";
    html += "<div class='form-group'><label>Speed True:</label><input type='text' name='env_wst' value='" + config.environment_wind_speed_true + "'></div>";
    html += "</div>";

    html += "<button type='button' class='nested-toggle expanded' onclick='toggleNested(event)'>Depth</button>";
    html += "<div class='nested-content expanded'>";
    html += "<div class='form-group'><label>Below Keel:</label><input type='text' name='env_dbk' value='" + config.environment_depth_below_keel + "'></div>";
    html += "<div class='form-group'><label>Below Transducer:</label><input type='text' name='env_dbt' value='" + config.environment_depth_below_transducer + "'></div>";
    html += "<div class='form-group'><label>Below Surface:</label><input type='text' name='env_dbs' value='" + config.environment_depth_below_surface + "'></div>";
    html += "</div>";

    html += "<button type='button' class='nested-toggle expanded' onclick='toggleNested(event)'>Outside</button>";
    html += "<div class='nested-content expanded'>";
    html += "<div class='form-group'><label>Pressure:</label><input type='text' name='env_press' value='" + config.environment_outside_pressure + "'></div>";
    html += "<div class='form-group'><label>Humidity:</label><input type='text' name='env_humid' value='" + config.environment_outside_humidity + "'></div>";
    html += "<div class='form-group'><label>Temperature:</label><input type='text' name='env_temp' value='" + config.environment_outside_temperature + "'></div>";
    html += "<div class='form-group'><label>Illuminance:</label><input type='text' name='env_illum' value='" + config.environment_outside_illuminance + "'></div>";
    html += "</div>";
    html += "                </div></div>";

    // Steering section
    html += R"(
            <div class='tree-item'>
                <button type='button' class='tree-toggle expanded' onclick='toggleTree(this)'>Steering</button>
                <div class='tree-content expanded'>
)";
    html += "<div class='form-group'><label>Rudder Angle:</label><input type='text' name='steer_rudder' value='" + config.steering_rudder_angle + "'></div>";
    html += "                </div></div>";

    // Vessel section
    html += R"(
            <div class='tree-item'>
                <button type='button' class='tree-toggle expanded' onclick='toggleTree(this)'>Vessel</button>
                <div class='tree-content expanded'>
)";
    html += "<button type='button' class='nested-toggle expanded' onclick='toggleNested(event)'>Design</button>";
    html += "<div class='nested-content expanded'>";
    html += "<div class='form-group'><label>Beam API:</label><input type='text' name='vessel_beam' value='" + config.vessel_design_beam_api + "'></div>";
    html += "<div class='form-group'><label>Air Height API:</label><input type='text' name='vessel_air' value='" + config.vessel_design_air_height_api + "'></div>";
    html += "<div class='form-group'><label>Draft API:</label><input type='text' name='vessel_draft' value='" + config.vessel_design_draft_api + "'></div>";
    html += "<div class='form-group'><label>Length API:</label><input type='text' name='vessel_len' value='" + config.vessel_design_length_api + "'></div>";
    html += "</div>";
    html += "<div class='form-group'><label>Name API:</label><input type='text' name='vessel_name' value='" + config.vessel_name_api + "'></div>";
    html += "<div class='form-group'><label>MMSI API:</label><input type='text' name='vessel_mmsi' value='" + config.vessel_mmsi_api + "'></div>";
    html += "<div class='form-group'><label>Navigation State API:</label><input type='text' name='vessel_nav' value='" + config.vessel_navigation_state_api + "'></div>";
    html += "                </div></div>";

    // Propulsion/Engines section
    html += R"(
            <div class='tree-item'>
                <button type='button' class='tree-toggle expanded' onclick='toggleTree(this)'>Propulsion/Engines</button>
                <div class='tree-content expanded'>
)";
    for (int i = 0; i < 2; i++) {
        html += "<div class='form-group'><label>Engine " + String(i) + " Path:</label><input type='text' name='eng_path_" + String(i) + "' value='" + config.engine_paths[i] + "'></div>";
    }
    html += "                </div></div>";

    // Tanks section - Signal K paths only
    html += R"(
            <div class='tree-item'>
                <button type='button' class='tree-toggle expanded' onclick='toggleTree(this)'>Tanks</button>
                <div class='tree-content expanded'>
                    <div class='info-text'>
                        Tank paths: tanks.{fluid}.{index}.currentLevel<br>
                        Fluid keywords (lowercase): fuel, fresh_water/fresh, waste_water/grey_water/grey, black_water/black, lubrication/lube, live_well/livewell, gas
                    </div>
)";
    for (int i = 0; i < config.num_tanks; i++) {
        html += "<div class='form-group'>";
        html += "<label>Tank " + String(i) + " Path:</label>";
        html += "<input type='text' name='tank_path_" + String(i) + "' placeholder='e.g., tanks.fuel.0.currentLevel' value='" + config.tank_paths[i] + "'>";
        html += "</div>";
    }
    html += "                </div></div>";

    html += R"(
        </form>
        <div class='actions'>
            <a class='back-link' href='/'>← Back to Administration</a>
            <button type='button' id='submitBtn' class='btn-save' onclick='saveConfig()' disabled>No changes to save</button>
            <button class='btn-export' onclick='exportConfig()'>Export JSON</button>
            <button class='btn-reset' onclick='if(confirm("Reset all to defaults?")) resetConfig()'>Reset to Defaults</button>
        </div>
    </div>
</div>
<script>
function toggleTree(button) {
    event.preventDefault();
    button.classList.toggle('expanded');
    const content = button.nextElementSibling;
    content.classList.toggle('expanded');
}

function toggleNested(event) {
    event.preventDefault();
    const button = event.target;
    button.classList.toggle('expanded');
    const content = button.nextElementSibling;
    content.classList.toggle('expanded');
}

window.originalValues = {};
window.changeCount = 0;
window.initChangeTracking = function() {
  console.log('initChangeTracking called');
  originalValues = {};
  const inputs = document.querySelectorAll('input, select');
  console.log('Found inputs:', inputs.length);
  inputs.forEach((input, idx) => {
    const name = input.name;
    if (name) {
      originalValues[name] = input.value.trim();
      console.log('Input', idx, name, ':', originalValues[name]);
      input.addEventListener('input', updateSubmitButton);
      input.addEventListener('change', updateSubmitButton);
      // Visual feedback
      input.style.borderColor = '#ddd';
    }
  });
  updateSubmitButton();
}

window.hasChanges = function() {
  const inputs = document.querySelectorAll('input, select');
  for (let input of inputs) {
    const name = input.name;
    if (name && input.value.trim() !== originalValues[name]) {
      return true;
    }
  }
  return false;
}

window.updateSubmitButton = function() {
  console.log('updateSubmitButton called');
  const btn = document.getElementById('submitBtn');
  if (!btn) {
    console.log('No submitBtn found');
    return;
  }
  const has = window.hasChanges();
  btn.disabled = !has;
  // Count
  let count = 0;
  const inputs = document.querySelectorAll('input, select');
  for (let input of inputs) {
    const name = input.name;
    if (name && input.value.trim() !== originalValues[name]) {
      count++;
      console.log('Changed:', name, input.value.trim(), 'vs', originalValues[name]);
    }
  }
  console.log('Change count:', count, 'hasChanges:', has);
  btn.textContent = has ? `Save ${count} change${count>1?'s':''}` : 'No changes (button disabled)';
  // Visual: highlight changed inputs
  inputs.forEach(input => {
    const name = input.name;
    if (name && input.value.trim() !== originalValues[name]) {
      input.style.borderColor = '#4CAF50';
      input.style.boxShadow = '0 0 0 2px rgba(76,175,80,0.2)';
    } else {
      input.style.borderColor = '#ddd';
      input.style.boxShadow = 'none';
    }
  });
}

window.restoreOriginals = function() {
  Object.keys(originalValues).forEach(name => {
    const input = document.querySelector(`[name="${name}"]`);
    if (input) {
      input.value = originalValues[name];
      input.dispatchEvent(new Event('input'));
    }
  });
}

async function saveConfig() {
    const changedData = {};
    const inputs = document.querySelectorAll('input, select');
    for (let input of inputs) {
        const name = input.name;
        let changed;
        changed = input.value.trim() !== originalValues[name];

        if (name && changed) {
            if (input.value.trim() !== originalValues[name]) {
                changedData[name] = input.value;
            }
        }
    }
    if (Object.keys(changedData).length === 0) {
        alert('No changes to save!');
        return;
    }
    
    try {
        const response = await fetch('/signalk-config/save', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(changedData)
        });
        if (response.ok) {
            alert(`Saved ${Object.keys(changedData).length} changed field${Object.keys(changedData).length > 1 ? 's' : ''} successfully!`);
            location.reload();
        } else {
            const text = await response.text();
            alert('Error saving configuration: ' + response.status + ' ' + text);
            console.error('Save error:', response.status, text);
        }
    } catch (error) {
        alert('Error: ' + error.message);
        console.error('Fetch error:', error);
    }
}

async function exportConfig() {
    const response = await fetch('/signalk-config/export');
    const data = await response.json();
    const json = JSON.stringify(data, null, 2);
    const blob = new Blob([json], {type: 'application/json'});
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'signalk-config.json';
    a.click();
}

async function resetConfig() {
    const response = await fetch('/signalk-config/reset', {method: 'POST'});
    if (response.ok) {
        location.reload();
    }
}

document.addEventListener('DOMContentLoaded', function() {
  console.log('DOM loaded, initializing change tracking');
  window.initChangeTracking();
});
 </script>
 </body>
 </html>
 )";

    web_server.send(200, "text/html", html);
}

void handle_show_display_config_page() {
    ESP_LOGI("WS", "[DEBUG] handle_show_display_config_page: rendering display config page\n");
    String html;
    html.reserve(8000);
    html += R"(<!DOCTYPE html>
<html>
<head>
<meta charset='utf-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Display Configuration</title>
<style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        background: linear-gradient(135deg, #2a5298 0%, #1e3c72 100%);
        color: #333;
        min-height: 100vh;
        padding: 20px;
    }
    .container {
        max-width: 800px;
        margin: 0 auto;
        background: white;
        border-radius: 12px;
        box-shadow: 0 20px 60px rgba(0,0,0,0.3);
        overflow: hidden;
    }
    .header {
        background: linear-gradient(135deg, #2a5298 0%, #1e3c72 100%);
        color: white;
        padding: 30px;
        text-align: center;
    }
    .header h1 { font-size: 2em; margin-bottom: 10px; }
    .content { padding: 30px; }
    .section {
        margin: 25px 0;
        padding: 20px;
        background: #f5f5f5;
        border-radius: 8px;
        border-left: 4px solid #4CAF50;
    }
    .section h2 {
        font-size: 1.2em;
        margin-bottom: 15px;
        color: #333;
    }
    .form-group {
        margin-bottom: 15px;
    }
    label {
        display: block;
        font-weight: 600;
        margin-bottom: 5px;
        color: #333;
        font-size: 0.95em;
    }
    input[type="text"],
    input[type="number"],
    select {
        width: 100%;
        padding: 10px;
        border: 1px solid #ddd;
        border-radius: 6px;
        font-size: 0.95em;
    }
    input:focus,
    select:focus {
        outline: none;
        border-color: #2a5298;
        box-shadow: 0 0 0 3px rgba(42, 82, 152, 0.1);
    }
    .actions {
        display: flex;
        gap: 12px;
        margin-top: 30px;
        flex-wrap: wrap;
    }
    button, a.back-link {
        padding: 12px 24px;
        border: none;
        border-radius: 6px;
        font-size: 1em;
        font-weight: 600;
        cursor: pointer;
        transition: all 0.2s;
    }
    .btn-save {
        background: #4CAF50;
        color: white;
    }
    .btn-save:hover { background: #45a049; transform: translateY(-2px); box-shadow: 0 4px 12px rgba(76, 175, 80, 0.3); }
    .back-link {
        display: inline-block;
        background: #f5f5f5;
        border: 1px solid #ddd;
        text-decoration: none;
        color: #333;
    }
    .back-link:hover { background: #efefef; }
</style>
</head>
<body>
<div class='container'>
    <div class='header'>
        <h1>Display Settings</h1>
        <p>Customize gauges, units, and historical data display</p>
    </div>
    <div class='content'>
        <form id='displayForm'>
            <div class='section'>
                <h2>Unit of Measurement</h2>
                <div class='form-group'>
                    <label>Distance Unit:</label>
                    <select name='dist_unit'>
                        <option value='0' )";
    html += (config.distance_unit == DistanceUnit::Meters) ? "selected" : "";
    html += R"(>Meters</option>
                        <option value='1' )";
    html += (config.distance_unit == DistanceUnit::Feet) ? "selected" : "";
    html += R"(>Feet</option>
                    </select>
                </div>
                <div class='form-group'>
                    <label>Temperature Unit:</label>
                    <select name='temp_unit'>
                        <option value='0' )";
    html += (config.temperature_unit == TemperatureUnit::Celsius) ? "selected" : "";
    html += R"(>Celsius (°C)</option>
                        <option value='1' )";
    html += (config.temperature_unit == TemperatureUnit::Fahrenheit) ? "selected" : "";
    html += R"(>Fahrenheit (°F)</option>
                    </select>
                </div>
            </div>

            <div class='section'>
                <h2>Engine Gauges</h2>
                <div class='form-group'><label>Number of Engines:</label><input type='number' name='num_engines' value=')";
    html += String(config.num_engines);
    html += R"(' min='1' max='8'></div>
                <div class='form-group'>
                    <label style='display:flex; align-items:center; gap:10px;'>
                        <input type='checkbox' name='eng_oil_enabled' value='true' )";
    html += config.engine_oil_pressure_enabled ? "checked" : "";
    html += R"(> Show Oil Pressure Gauge
                    </label>
                </div>
                <div class='form-group'>
                    <label style='display:flex; align-items:center; gap:10px;'>
                        <input type='checkbox' name='eng_top_left_enabled' value='true' )";
    html += config.engine_top_left_enabled ? "checked" : "";
    html += R"(> Show Top Left Engine Metric
                    </label>
                    <select name='eng_top_left_metric'>
                        <option value='0' )";
    html += config.engine_top_left_metric == EngineTopLeftMetric::SOG ? "selected" : "";
    html += R"(>SOG (kt)</option>
                        <option value='1' )";
    html += config.engine_top_left_metric == EngineTopLeftMetric::ThrottlePercent ? "selected" : "";
    html += R"(>Throttle (%)</option>
                    </select>
                </div>
                <div class='form-group'>
                    <label style='display:flex; align-items:center; gap:10px;'>
                        <input type='checkbox' name='eng_top_right_enabled' value='true' )";
    html += config.engine_top_right_enabled ? "checked" : "";
    html += R"(> Show Top Right Engine Metric
                    </label>
                    <select name='eng_top_right_metric'>
                        <option value='0' )";
    html += config.engine_top_right_metric == EngineTopRightMetric::AlternatorVoltage ? "selected" : "";
    html += R"(>Alternator Voltage</option>
                        <option value='1' )";
    html += config.engine_top_right_metric == EngineTopRightMetric::BatteryVoltage ? "selected" : "";
    html += R"(>Battery Voltage</option>
                    </select>
                </div>
                <div id='oilPressureZoneFields' class='form-group' style='" )";
    html += config.engine_oil_pressure_enabled ? "" : "display:none;";
    html += R"('>
                    <label>Oil Pressure Green Zone Minimum (PSI):</label><input type='number' name='eng_oil_min' value=')";
    html += String(config.engine_oil_pressure_min, 1);
    html += R"(' step='0.1'>
                </div>
                <div id='oilPressureZoneMaxFields' class='form-group' style='" )";
    html += config.engine_oil_pressure_enabled ? "" : "display:none;";
    html += R"('>
                    <label>Oil Pressure Green Zone Maximum (PSI):</label><input type='number' name='eng_oil_max' value=')";
    html += String(config.engine_oil_pressure_max, 1);
    html += R"(' step='0.1'>
                </div>
                <div class='form-group'><label>Temperature Redline (°C):</label><input type='number' name='eng_temp_red' value=')";
    html += String(config.engine_temp_redline, 1);
    html += R"(' step='0.1'></div>
            </div>

            <div class='section'>
                <h2>Tank Display</h2>
                <div class='form-group'><label>Number of Tanks:</label><input type='number' name='num_tanks' value=')";
    html += String(config.num_tanks);
    html += R"(' min='1' max='8'></div>
                <p style='font-size: 0.9em; color: #666;'>Tank bar dimensions are automatically calculated based on the number of tanks configured.</p>
            </div>

            <div class='section'>
                <h2>Historical Chart Data</h2>
                <div class='form-group'><label>Depth Chart Duration (minutes):</label><input type='number' name='depth_chart_min' value=')";
    html += String(config.depth_chart_duration);
    html += R"(' min='5' max='120'></div>
                <div class='form-group'><label>Speed Chart Duration (minutes):</label><input type='number' name='speed_chart_min' value=')";
    html += String(config.speed_chart_duration);
    html += R"(' min='5' max='120'></div>
                <p style='font-size: 0.9em; color: #666;'><strong>The full chart width will display the configured duration of historical data.</strong></p>
            </div>)";

    html += R"(<div class='section'>
                <h2>Screens</h2>)";

    for (int i = 0; i < config.num_engines; i++) {
        char id[20];
        sprintf(id, "engine_%d", i);

        html += R"(<div>)";
        html += R"(<input type='checkbox' name='screen_)" + String(id) + R"(' )";

        if (is_screen_enabled(id)) {
            html += R"(checked)";
        }

        html += R"(> Engine )";
        html += String(i + 1);
        html += R"(</div>)";
    }

    // Static screens
    const char* static_screens[] = {
        "wind", "depth", "speed", "compass", "gps", "tanks", "heel"
    };

    for (int i = 0; i < 7; i++) {
        const char* id = static_screens[i];

        html += R"(<div>)";
        html += R"(<input type='checkbox' name='screen_)" + String(id) + R"(' )";

        if (is_screen_enabled(id)) {
            html += R"(checked)";
        }

        html += R"(> )";
        html += id;
        html += R"(</div>)";
    }

    // Reboot (locked)
    html += R"(<div>
        </div>
        </form>
        <div class='actions'>
            <a class='back-link' href='/'>← Back to Administration</a>
            <button type='button' id='submitBtn' class='btn-save' onclick='saveDisplayConfig()' disabled>No changes to save</button>
        </div>
    </div>
</div>
<script>
let originalValues = {};
let changeCount = 0;

function getValue(input) {
    if (input.type === 'checkbox') {
        return input.checked;  // boolean
    } else if (input.type === 'number') {
        return parseFloat(input.value);  // number
    } else if (input.tagName === 'SELECT') {
        return input.value;  // string (consistent)
    } else {
        return input.value.trim();  // string
    }
}

function updateOilPressureZoneVisibility() {
  const oilCheckbox = document.querySelector('input[name="eng_oil_enabled"]');
  const zoneFields = document.getElementById('oilPressureZoneFields');
  const zoneMaxFields = document.getElementById('oilPressureZoneMaxFields');

  if (!oilCheckbox || !zoneFields || !zoneMaxFields) return;

  const shouldShow = oilCheckbox.checked;
  zoneFields.style.display = shouldShow ? '' : 'none';
  zoneMaxFields.style.display = shouldShow ? '' : 'none';
}

function syncMetricCheckboxes() {
  const topLeftEnabled = document.querySelector('input[name="eng_top_left_enabled"]');
  const topLeftMetric = document.querySelector('select[name="eng_top_left_metric"]');
  const topRightEnabled = document.querySelector('input[name="eng_top_right_enabled"]');
  const topRightMetric = document.querySelector('select[name="eng_top_right_metric"]');

  if (topLeftEnabled && topLeftMetric) {
    const metricSelected = topLeftMetric.value === '1';
    const derivedChecked = topLeftEnabled.checked || metricSelected;
    topLeftEnabled.checked = derivedChecked;
    topLeftMetric.disabled = !topLeftEnabled.checked;
  }

  if (topRightEnabled && topRightMetric) {
    const metricSelected = topRightMetric.value === '1';
    const derivedChecked = topRightEnabled.checked || metricSelected;
    topRightEnabled.checked = derivedChecked;
    topRightMetric.disabled = !topRightEnabled.checked;
  }
}

function initChangeTracking() {
  originalValues = {};
  const inputs = document.querySelectorAll('input, select');
  inputs.forEach((input, idx) => {
    const name = input.name;
    if (name) {
      originalValues[name] = getValue(input);
      input.addEventListener('change', () => {
        syncMetricCheckboxes();
        updateSubmitButton();
      });
      input.style.borderColor = '#ddd';
    }
  });

  const oilCheckbox = document.querySelector('input[name="eng_oil_enabled"]');
  if (oilCheckbox) {
    oilCheckbox.addEventListener('change', updateOilPressureZoneVisibility);
  }

  syncMetricCheckboxes();
  updateOilPressureZoneVisibility();
  updateSubmitButton();
}

function hasChanges() {
    const inputs = document.querySelectorAll('input, select');

    for (let input of inputs) {
        const name = input.name;
        if (!name) continue;

        if (getValue(input) !== originalValues[name]) {
        return true;
        }
    }
    return false;
}

function updateSubmitButton() {
  const btn = document.getElementById('submitBtn');
  if (!btn) return;
  const has = hasChanges();
  btn.disabled = !has;
  let count = 0;
  const inputs = document.querySelectorAll('input, select');
  for (let input of inputs) {
    const name = input.name;
    if (getValue(input) !== originalValues[name]) {
      count++;
    }
  }
  btn.textContent = has ? `Save ${count} change${count>1?'s':''}` : 'No changes (disabled)';
  inputs.forEach(input => {
    const name = input.name;
    if (!name) return;

    let changed;
    if (input.type === 'checkbox') {
      changed = input.checked !== originalValues[name];
    } else if (input.type === 'number') {
      changed = parseFloat(input.value) !== parseFloat(originalValues[name]);
    } else {
      changed = input.value.trim() !== originalValues[name];
    }

    if (changed) {
      input.style.borderColor = '#4CAF50';
      input.style.boxShadow = '0 0 0 2px rgba(76,175,80,0.2)';
    } else {
      input.style.borderColor = '#ddd';
      input.style.boxShadow = 'none';
    }
  });
}

initChangeTracking();

async function saveDisplayConfig() {
    const changedData = {};
    const inputs = document.querySelectorAll('input, select');

    syncMetricCheckboxes();
    const topLeftCheckbox = document.querySelector('input[name="eng_top_left_enabled"]');
    const topLeftMetric = document.querySelector('select[name="eng_top_left_metric"]');
    const topRightCheckbox = document.querySelector('input[name="eng_top_right_enabled"]');
    const topRightMetric = document.querySelector('select[name="eng_top_right_metric"]');

    for (let input of inputs) {
        const name = input.name;
        if (!name) continue;

        let value = getValue(input);
        if (name === 'eng_top_left_enabled' && topLeftCheckbox) {
            value = topLeftCheckbox.checked;
        }
        if (name === 'eng_top_left_metric' && topLeftMetric) {
            value = topLeftMetric.value;
        }
        if (name === 'eng_top_right_enabled' && topRightCheckbox) {
            value = topRightCheckbox.checked;
        }
        if (name === 'eng_top_right_metric' && topRightMetric) {
            value = topRightMetric.value;
        }

        if (value !== originalValues[name]) {
            changedData[name] = value;
        }
    }

    if (Object.keys(changedData).length === 0) {
        alert('No changes to save!');
        return;
    }
    
    try {
        const json = JSON.stringify(changedData);

        const response = await fetch('/display-config/save', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: json
        });
        if (response.ok) {
            alert(`Saved ${Object.keys(changedData).length} changed field${Object.keys(changedData).length > 1 ? 's' : ''} successfully!`);
            location.reload();
        } else {
            const text = await response.text();
            alert('Error saving settings: ' + response.status + ' ' + text);
            console.error('Save error:', response.status, text);
        }
    } catch (error) {
        alert('Error: ' + error.message);
        console.error('Fetch error:', error);
    }
}
</script>
</body>
</html>
)";

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
