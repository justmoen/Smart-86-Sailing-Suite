#ifndef SIGNALK_PATH_CONFIG_H
#define SIGNALK_PATH_CONFIG_H

#include <Arduino.h>

enum class DistanceUnit { Meters, Feet };
enum class TemperatureUnit { Celsius, Fahrenheit };

struct signalk_path_config_t {
  // Navigation paths
  String navigation_rate_of_turn;
  String navigation_heading_magnetic;
  String navigation_position;
  String navigation_speed_over_ground;
  String navigation_speed_through_water;
  String navigation_course_over_ground_true;
  String navigation_course_rhumbline_cross_track_error;
  String navigation_course_rhumbline_bearing_track_true;
  String navigation_course_rhumbline_next_point_distance;
  String navigation_course_rhumbline_next_point_velocity_made_good;
  String navigation_state;
  String navigation_attitude_roll;
  String navigation_attitude_pitch;

  // Environment paths
  String environment_wind_angle_apparent;
  String environment_wind_angle_true_ground;
  String environment_wind_angle_true_water;
  String environment_wind_speed_apparent;
  String environment_wind_speed_over_ground;
  String environment_wind_speed_true;

  String environment_depth_below_keel;
  String environment_depth_below_transducer;
  String environment_depth_below_surface;

  String environment_outside_pressure;
  String environment_outside_humidity;
  String environment_outside_temperature;
  String environment_outside_illuminance;

  // Steering paths
  String steering_rudder_angle;

  // Vessel API paths
  String vessel_design_beam_api;
  String vessel_design_air_height_api;
  String vessel_design_draft_api;
  String vessel_design_length_api;
  String vessel_name_api;
  String vessel_mmsi_api;
  String vessel_navigation_state_api;

  // Propulsion/Engine paths (parameterized per engine)
  String engine_paths[8];  // Up to 8 engines: "propulsion.engines.0.temperature", etc

  // Tank paths (parameterized per tank)
  String tank_paths[8];    // Up to 8 tanks: "tanks.fuel.0.currentLevel", etc

  // DISPLAY Configuration (separate from Signal K paths)
  int num_engines = 1;                      // Number of engines to display (1-8)
  int num_tanks = 4;                        // Number of tanks to display (1-8), default 4
  
  // Engine screen configurations (engine_id = which engine index to display, 0-7)
  int engine_screen_1_id = 0;               // First engine screen displays engine 0 by default
  int engine_screen_2_id = 1;               // Second engine screen displays engine 1 by default
  
  // Engine gauge thresholds
  float engine_oil_pressure_min = 10.0f;    // PSI - min of green zone
  float engine_oil_pressure_max = 60.0f;    // PSI - max of green zone
  float engine_temp_redline = 100.0f;       // Celsius

  // Tank display scaling (for 720x720 screen) - optimized for readability
  int tank_bar_width = 80;                  // pixels - optimized for 4 tanks
  int tank_bar_height = 100;                // pixels - optimized for 4 tanks

  // Chart configuration (in minutes)
  int depth_chart_duration = 10;            // 10 minute default
  int speed_chart_duration = 30;            // 30 minute default
  
  // Unit measurement preferences
  DistanceUnit distance_unit = DistanceUnit::Meters;  // or Feet
  TemperatureUnit temperature_unit = TemperatureUnit::Celsius;  // or Fahrenheit
};

const signalk_path_config_t& get_signalk_path_config();
void load_signalk_path_config();
void signalk_path_config_web_begin();
void signalk_path_config_web_loop();

#endif
