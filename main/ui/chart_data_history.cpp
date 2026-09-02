#include "chart_data_history.h"

ChartDataHistory::ChartDataHistory(const char* key, int default_duration_min, int point_count)
    : namespace_key(String(kNamespacePrefix) + String(key)),
      write_index(0),
      total_points(0),
      start_time_ms(millis()),
      last_sample_ms(0),
      duration_minutes(default_duration_min) {
    
    // Calculate point interval based on duration and point count
    if (point_count > 0) {
        point_interval_ms = (duration_minutes * 60000LL) / point_count;
        if (point_interval_ms < 100) {
            point_interval_ms = 100;
        }
    } else {
        point_interval_ms = 2000;  // 2s fallback for smooth scrolling
    }
    
#ifdef CONFIG_LOG_DEFAULT_LEVEL_INFO
    Serial.printf("ChartHistory(%s,%dmin,%dpts): interval=%ums\n", key, duration_minutes, point_count, (unsigned)point_interval_ms);
#endif
    
    // Load saved duration from NVS
    Preferences prefs;
    prefs.begin(namespace_key.c_str(), true);
    duration_minutes = prefs.getInt("duration", default_duration_min);
    prefs.end();
    
    // Recalculate after load
    if (point_count > 0) {
        point_interval_ms = (duration_minutes * 60000LL) / point_count;
        if (point_interval_ms < 100) {
            point_interval_ms = 100;
        }
    }
    
    load_from_nv();
}

void ChartDataHistory::add_point(float value) {
    uint32_t now = millis();
    
    if (now - last_sample_ms < point_interval_ms) {
#ifdef CONFIG_LOG_DEFAULT_LEVEL_INFO
        static uint32_t last_log = 0;
        if (millis() - last_log > 30000) {
            Serial.printf("Chart %s throttle skip (need %ums, have %ums)\n", namespace_key.c_str(), point_interval_ms, now - last_sample_ms);
            last_log = millis();
        }
#endif
        return;
    }
    
    last_sample_ms = now;
    
    data[write_index].value = value;
    data[write_index].timestamp_ms = now - start_time_ms;
    
    write_index = (write_index + 1) % CHART_MAX_POINTS;
    if (total_points < CHART_MAX_POINTS) {
        total_points++;
    }
}

void ChartDataHistory::get_points(ChartDataPoint* out_buffer, int& out_count, int max_points) {
    uint32_t now = millis();
    uint32_t window_ms = (uint32_t)duration_minutes * 60 * 1000;
    uint32_t cutoff_time = (now - start_time_ms) - window_ms;
    
    out_count = 0;
    
    for (int i = 0; i < total_points && out_count < max_points; i++) {
        int idx = (write_index - 1 - i + CHART_MAX_POINTS) % CHART_MAX_POINTS;
        if (data[idx].timestamp_ms < cutoff_time) {
            break;
        }
        out_buffer[out_count++] = data[idx];
    }
    
    // Reverse to chronological
    for (int i = 0; i < out_count / 2; i++) {
        ChartDataPoint temp = out_buffer[i];
        out_buffer[i] = out_buffer[out_count - 1 - i];
        out_buffer[out_count - 1 - i] = temp;
    }
}

void ChartDataHistory::clear() {
    write_index = 0;
    total_points = 0;
    start_time_ms = millis();
    last_sample_ms = start_time_ms;
    memset(data, 0, sizeof(data));
}

void ChartDataHistory::load_from_nv() {
    Preferences prefs;
    prefs.begin(namespace_key.c_str(), true);
    
    total_points = prefs.getInt("count", 0);
    write_index = prefs.getInt("writeIdx", 0);
    
    if (prefs.isKey("data")) {
        size_t read = prefs.getBytes("data", (uint8_t*)data, sizeof(data));
        if (read != sizeof(data)) {
            total_points = 0;
        }
    }
    
    start_time_ms = millis();
    last_sample_ms = start_time_ms;
    
    prefs.end();
}

void ChartDataHistory::save_to_nv() {
    Preferences prefs;
    prefs.begin(namespace_key.c_str(), false);
    
    prefs.putInt("count", total_points);
    prefs.putInt("writeIdx", write_index);
    prefs.putBytes("data", (uint8_t*)data, sizeof(data));
    prefs.putInt("duration", duration_minutes);
    
    prefs.end();
}

int ChartDataHistory::get_duration_minutes() const {
    return duration_minutes;
}

void ChartDataHistory::set_duration_minutes(int minutes) {
    duration_minutes = minutes;
    Preferences prefs;
    prefs.begin(namespace_key.c_str(), false);
    prefs.putInt("duration", minutes);
    prefs.end();
}


