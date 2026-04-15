#include "signalk_path_config.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <WebServer.h>

constexpr const char* kPrefsNamespace = "sk-config";

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
    Preferences prefs;
    prefs.begin(kPrefsNamespace, true);
    float val = prefs.getFloat(key, default_val);
    prefs.end();
    return val;
}

// Save float to preferences
void save_float_pref(const char* key, float value) {
    // Check if value has changed (use epsilon for float comparison)
    float current = load_float_pref(key, -999999.0f);
    if (abs(value - current) < 0.0001f) {
        return;  // No change, skip write
    }
    Preferences prefs;
    prefs.begin(kPrefsNamespace, false);
    prefs.putFloat(key, value);
    prefs.end();
}

// Load int from preferences with default
int load_int_pref(const char* key, int default_val) {
    Preferences prefs;
    prefs.begin(kPrefsNamespace, true);
    int val = prefs.getInt(key, default_val);
    prefs.end();
    return val;
}

// Save int to preferences
void save_int_pref(const char* key, int value) {
    // Check if value has changed
    int current = load_int_pref(key, -999999);
    if (value == current) {
        return;  // No change, skip write
    }
    Preferences prefs;
    prefs.begin(kPrefsNamespace, false);
    prefs.putInt(key, value);
    prefs.end();
}

// Load string from preferences with default
String load_string_pref(const char* key, const char* default_val) {
    Preferences prefs;
    prefs.begin(kPrefsNamespace, true);
    String val = prefs.getString(key, default_val);
    prefs.end();
    return val;
}

// Save string to preferences
void save_string_pref(const char* key, const String& value) {
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
    oilPressure["minPSI"] = config.engine_oil_pressure_min;
    oilPressure["maxPSI"] = config.engine_oil_pressure_max;
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
    </div>
</div>
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
            <button class='btn-save' onclick='saveConfig()'>Save Configuration</button>
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

async function saveConfig() {
    const form = document.getElementById('configForm');
    const formData = new FormData(form);
    
    // Convert FormData to JSON object
    const jsonData = {};
    for (const [key, value] of formData.entries()) {
        jsonData[key] = value;
    }
    
    try {
        const response = await fetch('/signalk-config/save', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(jsonData)
        });
        if (response.ok) {
            alert('Configuration saved successfully!');
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
</script>
</body>
</html>
)";

    web_server.send(200, "text/html", html);
}

void handle_show_display_config_page() {
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
                <div class='form-group'><label>Oil Pressure Green Zone Minimum (PSI):</label><input type='number' name='eng_oil_min' value=')";
    html += String(config.engine_oil_pressure_min, 1);
    html += R"(' step='0.1'></div>
                <div class='form-group'><label>Oil Pressure Green Zone Maximum (PSI):</label><input type='number' name='eng_oil_max' value=')";
    html += String(config.engine_oil_pressure_max, 1);
    html += R"(' step='0.1'></div>
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
            </div>
        </form>
        <div class='actions'>
            <a class='back-link' href='/'>← Back to Administration</a>
            <button class='btn-save' onclick='saveDisplayConfig()'>Save Settings</button>
        </div>
    </div>
</div>
<script>
async function saveDisplayConfig() {
    const form = document.getElementById('displayForm');
    const formData = new FormData(form);
    
    // Convert FormData to JSON object
    const jsonData = {};
    for (const [key, value] of formData.entries()) {
        jsonData[key] = value;
    }
    
    try {
        const response = await fetch('/display-config/save', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(jsonData)
        });
        if (response.ok) {
            alert('Display settings saved successfully!');
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
    JsonDocument doc;  // Large buffer for 50+ fields (auto-sized)
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        web_server.send(400, "text/plain", "Invalid JSON");
        return;
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
        web_server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    // Load display configuration fields
    if (doc["dist_unit"].is<int>()) {
        config.distance_unit = (doc["dist_unit"].as<int>() == 0) ? DistanceUnit::Meters : DistanceUnit::Feet;
    }
    if (doc["temp_unit"].is<int>()) {
        config.temperature_unit = (doc["temp_unit"].as<int>() == 0) ? TemperatureUnit::Celsius : TemperatureUnit::Fahrenheit;
    }
    if (doc["num_engines"].is<int>()) {
        config.num_engines = doc["num_engines"].as<int>();
        if (config.num_engines > 8) config.num_engines = 8;
        if (config.num_engines < 1) config.num_engines = 1;
    }
    if (doc["eng_oil_min"].is<float>()) config.engine_oil_pressure_min = doc["eng_oil_min"].as<float>();
    if (doc["eng_oil_max"].is<float>()) config.engine_oil_pressure_max = doc["eng_oil_max"].as<float>();
    if (doc["eng_temp_red"].is<float>()) config.engine_temp_redline = doc["eng_temp_red"].as<float>();
    if (doc["num_tanks"].is<int>()) {
        config.num_tanks = doc["num_tanks"].as<int>();
        if (config.num_tanks > 8) config.num_tanks = 8;
        if (config.num_tanks < 1) config.num_tanks = 1;
    }
    if (doc["depth_chart_min"].is<int>()) config.depth_chart_duration = doc["depth_chart_min"].as<int>();
    if (doc["speed_chart_min"].is<int>()) config.speed_chart_duration = doc["speed_chart_min"].as<int>();

    // Save all to preferences
    save_all_config_to_preferences();

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

const signalk_path_config_t& get_signalk_path_config() {
    return config;
}

void load_signalk_path_config() {
    load_config_from_preferences();
}

void signalk_path_config_web_begin() {
    if (web_server_started) {
        return;
    }

    load_config_from_preferences();
    // Admin index page
    web_server.on("/", HTTP_GET, handle_show_admin_index);
    
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
