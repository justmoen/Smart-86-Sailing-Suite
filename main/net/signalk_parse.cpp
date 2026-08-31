#include "signalk_parse.h"
#include <ship_data_model.h>
#include <ArduinoJson.h>
#include <TinyGPSPlus.h>
#include <ship_data_util.h>
#include "signalk_path_config.h"
#include <ship_data_model.h>

#ifdef __cplusplus
extern "C" {
#endif

// Extract lowercase {fluid} from tanks.{fluid}.{index}.currentLevel
static String get_fluid_from_path(const String& path) {
  int tanks_pos = path.indexOf("tanks.");
  if (tanks_pos == -1) return "";
  int fluid_start = tanks_pos + 6;
  int fluid_end = path.indexOf('.', fluid_start);
  if (fluid_end == -1) return "";
  String fluid = path.substring(fluid_start, fluid_end);
  fluid.toLowerCase();
  return fluid;
}

// Map lowercase fluid keyword to fluid_type_e
static fluid_type_e string_to_fluid_type(const String& fluid_lower) {
  if (fluid_lower == "fuel") return fluid_type_e::FUEL;
  if (fluid_lower == "fresh_water" || fluid_lower == "fresh") return fluid_type_e::FRESH_WATER;
  if (fluid_lower == "waste_water" || fluid_lower == "grey_water" || fluid_lower == "grey") return fluid_type_e::WASTE_WATER;
  if (fluid_lower == "black_water" || fluid_lower == "black") return fluid_type_e::BLACK_WATER;
  if (fluid_lower == "lubrication" || fluid_lower == "lube") return fluid_type_e::LUBRICATION;
  if (fluid_lower == "live_well" || fluid_lower == "livewell") return fluid_type_e::LIVE_WELL;
  if (fluid_lower == "gas") return fluid_type_e::GAS;
  return fluid_type_e::FLUID_TYPE_NA;
}

  void set_vessel_nav_state(String& val) {
    if (val == "moored") {
      shipDataModel.navigation.state.st = nav_state_e::NS_MOORED;
      shipDataModel.navigation.state.age = millis();
    } else if (val == "sailing") {
      shipDataModel.navigation.state.st = nav_state_e::NS_SAILING;
      shipDataModel.navigation.state.age = millis();
    } else if (val == "motoring") {
      shipDataModel.navigation.state.st = nav_state_e::NS_MOTORING;
      shipDataModel.navigation.state.age = millis();
    } else if (val == "anchored") {
      shipDataModel.navigation.state.st = nav_state_e::NS_ANCHORED;
      shipDataModel.navigation.state.age = millis();
    }
  }

  bool signalk_parse(const char* payload, size_t length) {
    bool found = false;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    // Parse succeeded?
    if (err) {
      ESP_LOGE("PARSE", "JSON failed: %s", err.c_str());
      return false;
    }
    JsonObject obj = doc.as<JsonObject>();
    if (obj != NULL) {
      const auto& config = get_signalk_path_config();
      auto update_value = [&](String& path, size_t& u_idx, size_t& v_idx, JsonVariant& value) {
        const char* p = path.c_str();
        String enginePrefix = "propulsion.engines.";
        if (path == config.navigation_rate_of_turn) {
          if (value.is<float>()) {
            shipDataModel.navigation.rate_of_turn.deg_min = 60 * value.as<float>() * 180 / PI;
            shipDataModel.navigation.rate_of_turn.age = millis();
          }
        } else if (path == config.navigation_heading_magnetic) {
          if (value.is<float>()) {
            shipDataModel.navigation.heading_mag.deg = value.as<float>() * 180.0 / PI;
            shipDataModel.navigation.heading_mag.age = millis();
          }
        } else if (path == config.navigation_position) {
          if (!value["longitude"].isNull() && !value["latitude"].isNull()) {
            if (value["longitude"].is<float>() && value["latitude"].is<float>()) {
              shipDataModel.navigation.position.lat.deg = value["latitude"].as<float>();
              shipDataModel.navigation.position.lat.age = millis();
              shipDataModel.navigation.position.lon.deg = value["longitude"].as<float>();
              shipDataModel.navigation.position.lon.age = millis();
            }
          }
        } else if (path == config.navigation_speed_over_ground) {
          if (value.is<float>()) {
            shipDataModel.navigation.speed_over_ground.kn = value.as<float>() / _GPS_MPS_PER_KNOT;
            shipDataModel.navigation.speed_over_ground.age = millis();
          }
        } else if (path == config.navigation_speed_through_water) {
          if (value.is<float>()) {
            shipDataModel.navigation.speed_through_water.kn = value.as<float>() / _GPS_MPS_PER_KNOT;
            shipDataModel.navigation.speed_through_water.age = millis();
          }
        } else if (path == config.navigation_course_over_ground_true) {
          if (value.is<float>()) {
            shipDataModel.navigation.course_over_ground_true.deg = value.as<float>() * 180.0 / PI;
            shipDataModel.navigation.course_over_ground_true.age = millis();
          }
        } else if (path == config.navigation_course_rhumbline_cross_track_error) {
          if (value.is<float>()) {
            shipDataModel.navigation.course_rhumbline.cross_track_error.m = value.as<float>();
            shipDataModel.navigation.course_rhumbline.cross_track_error.age = millis();
          }
        } else if (path == config.navigation_course_rhumbline_bearing_track_true) {
          if (value.is<float>()) {
            shipDataModel.navigation.course_rhumbline.bearing_track_true.deg = value.as<float>() * 180.0 / PI;
            shipDataModel.navigation.course_rhumbline.bearing_track_true.age = millis();
          }
        } else if (path == config.navigation_course_rhumbline_next_point_distance) {
          if (value.is<float>()) {
            shipDataModel.navigation.course_rhumbline.next_point.distance.m = value.as<float>();
            shipDataModel.navigation.course_rhumbline.next_point.distance.age = millis();
          }
        } else if (path == config.navigation_course_rhumbline_next_point_velocity_made_good) {
          if (value.is<float>()) {
            shipDataModel.navigation.course_rhumbline.next_point.velocity_made_good.kn =
                value.as<float>() / _GPS_MPS_PER_KNOT;
            shipDataModel.navigation.course_rhumbline.next_point.velocity_made_good.age = millis();
          }
        } else if (path == config.navigation_state) {
          if (value.is<String>()) {
            String val = value.as<String>();
            if (val != NULL) {
              set_vessel_nav_state(val);
            }
          }
        } else if (path == config.navigation_attitude_roll) {
          if (value.is<float>()) {
            shipDataModel.navigation.attitude.heel.deg = value.as<float>() * 180.0 / PI;
            shipDataModel.navigation.attitude.heel.age = millis();
          }
        } else if (path == config.navigation_attitude_pitch) {
          if (value.is<float>()) {
            shipDataModel.navigation.attitude.pitch.deg = value.as<float>() * 180.0 / PI;
            shipDataModel.navigation.attitude.pitch.age = millis();
          }
        } else if (path == config.environment_wind_angle_apparent) {
          if (value.is<float>()) {
            shipDataModel.environment.wind.apparent_wind_angle.deg = value.as<float>() * 180.0 / PI;
            shipDataModel.environment.wind.apparent_wind_angle.age = millis();
          }
        } else if (path == config.environment_wind_angle_true_ground) {
          if (value.is<float>()) {
            shipDataModel.environment.wind.ground_wind_angle.deg = value.as<float>() * 180.0 / PI;
            shipDataModel.environment.wind.ground_wind_angle.age = millis();
          }
        } else if (path == config.environment_wind_angle_true_water) {
          if (value.is<float>()) {
            shipDataModel.environment.wind.true_wind_angle.deg = value.as<float>() * 180.0 / PI;
            shipDataModel.environment.wind.true_wind_angle.age = millis();
          }
        } else if (path == config.environment_wind_speed_apparent) {
          if (value.is<float>()) {
            shipDataModel.environment.wind.apparent_wind_speed.kn = value.as<float>() / _GPS_MPS_PER_KNOT;
            shipDataModel.environment.wind.apparent_wind_speed.age = millis();
          }
        } else if (path == config.environment_wind_speed_over_ground) {
          if (value.is<float>()) {
            shipDataModel.environment.wind.ground_wind_speed.kn = value.as<float>() / _GPS_MPS_PER_KNOT;
            shipDataModel.environment.wind.ground_wind_speed.age = millis();
          }
        } else if (path == config.environment_wind_speed_true) {
          if (value.is<float>()) {
            shipDataModel.environment.wind.true_wind_speed.kn = value.as<float>() / _GPS_MPS_PER_KNOT;
            shipDataModel.environment.wind.true_wind_speed.age = millis();
          }
        } else if (path == config.environment_depth_below_keel) {
          if (value.is<float>()) {
            shipDataModel.environment.depth.below_keel.m = value.as<float>();
            shipDataModel.environment.depth.below_keel.age = millis();
          }
        } else if (path == config.environment_depth_below_transducer) {
          if (value.is<float>()) {
            shipDataModel.environment.depth.below_transducer.m = value.as<float>();
            shipDataModel.environment.depth.below_transducer.age = millis();
          }
        } else if (path == config.environment_depth_below_surface) {
          if (value.is<float>()) {
            shipDataModel.environment.depth.below_surface.m = value.as<float>();
            shipDataModel.environment.depth.below_surface.age = millis();
          }
        } else if (path == config.environment_outside_pressure) {
          if (value.is<float>()) {
            shipDataModel.environment.air_outside.pressure.hPa = value.as<float>() / 100.0;
            shipDataModel.environment.air_outside.pressure.age = millis();
          }
        } else if (path == config.environment_outside_humidity) {
          if (value.is<float>()) {
            shipDataModel.environment.air_outside.humidity_pct.pct = value.as<float>() * 100.0;
            shipDataModel.environment.air_outside.humidity_pct.age = millis();
          }
        } else if (path == config.environment_outside_temperature) {
          if (value.is<float>()) {
            shipDataModel.environment.air_outside.temp_deg_C.deg_C = value.as<float>() - 273.15;
            shipDataModel.environment.air_outside.temp_deg_C.age = millis();
          }
        } else if (path == config.environment_outside_illuminance) {
          if (value.is<float>()) {
            shipDataModel.environment.air_outside.illuminance.lux = value.as<float>();
            shipDataModel.environment.air_outside.illuminance.age = millis();
          }
        } else if (path == config.steering_rudder_angle) {
          if (value.is<float>()) {
            shipDataModel.steering.rudder_angle.deg = value.as<float>() * 180.0 / PI;
            shipDataModel.steering.rudder_angle.age = millis();
          }
        } else if (starts_with(p, enginePrefix.c_str())) {
          String remainder = path.substring(enginePrefix.length());
          int dotIndex = remainder.indexOf('.');
          if (dotIndex == -1) return;
          String engineID = remainder.substring(0, dotIndex);
          engineID.toLowerCase();
          engine_t* eng = lookup_engine(engineID.c_str());
          if (eng == NULL) return;
          String field = remainder.substring(dotIndex + 1);
          if (field == "revolutions" || field == "rpm") {
            if (value.is<float>()) {
              eng->revolutions_RPM.rpm = value.as<float>() * 60;
              eng->revolutions_RPM.age = millis();
            }
          } else if (field == "temperature") {
            if (value.is<float>()) {
              eng->temp_deg_C.deg_C = value.as<float>() - 273.15;
              eng->temp_deg_C.age = millis();
            }
          } else if (field == "actualCurrent") {
            if (value.is<float>()) {
              eng->actual_current.amp = value.as<float>();
              eng->actual_current.age = millis();
            }
          } else if (field == "targetCurrent") {
            if (value.is<float>()) {
              eng->target_current.amp = value.as<float>();
              eng->target_current.age = millis();
            }
          } else if (field == "batteries.voltage") {
            if (value.is<float>()) {
              eng->battery_voltage.volt = value.as<float>();
              eng->battery_voltage.age = millis();
            }
          } else if (field == "throttle") {
            if (value.is<float>()) {
              float throttle = value.as<float>();
              if (throttle <= 1.0f) throttle *= 100.0f;
              eng->throttle.pct = throttle;
              eng->throttle.age = millis();
            }
          } else if (field == "oilPressure") {
            if (value.is<float>()) {
              eng->oil_pressure.hPa = value.as<float>() / 100.0;
              eng->oil_pressure.age = millis();
            }
          } else if (field == "alternatorVoltage" || field == "alternator.voltage") {
            if (value.is<float>()) {
              eng->alternator_voltage.volt = value.as<float>();
              eng->alternator_voltage.age = millis();
            }
          }
        } else if (starts_with(p, "tanks.")) {
          // Tank parsing: tanks.{fluid}.{index}.currentLevel → model.tank[index].percent_of_full & fluid_type from {fluid}
          const auto& config = get_signalk_path_config();
          for (int i = 0; i < MAX_TANKS; i++) {
            if (path == config.tank_paths[i] && value.is<float>()) {
              shipDataModel.tanks.tank[i].percent_of_full.pct = value.as<float>() * 100.0f;
              shipDataModel.tanks.tank[i].percent_of_full.age = millis();
              // Extract {fluid} keyword, map to enum
              String fluid_lower = get_fluid_from_path(path);
              shipDataModel.tanks.tank[i].fluid_type = string_to_fluid_type(fluid_lower);
              found = true;
              break;
            }
          }
        }
      };

      JsonArray updates = obj["updates"];
      if (!updates.isNull()) {
        for (size_t i_u = 0; i_u < updates.size(); i_u++) {
          JsonObject update = updates[i_u];
          if (!update.isNull()) {
            JsonArray values = update["values"];
            if (!values.isNull()) {
              for (size_t i_v = 0; i_v < values.size(); i_v++) {
                JsonObject valueObj = values[i_v];
                if (!valueObj.isNull()) {
                  if (!valueObj["path"].isNull()) {
                    String path = valueObj["path"].as<const char*>();
                    if (path != NULL) {
                      JsonVariant value = valueObj["value"];
                      if (!value.isNull()) {
                        update_value(path, i_u, i_v, value);
                        found = true;
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
    return found;
  }

  #ifdef __cplusplus
} /*extern "C"*/
#endif
