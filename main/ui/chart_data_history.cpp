#include "chart_data_history.h"

ChartDataHistory::ChartDataHistory(const char* key, int default_duration_min, int point_count)
    : namespace_key(String(kNamespacePrefix) + String(key)),
      write_index(0),
      total_points(0),
      start_time_ms(millis()),
      last_sample_ms(0),
      duration_minutes(default_duration_min) {
    
    // Calculate point interval based on duration and point count
    // Ensures that all data points fit within the specified duration
    if (point_count > 0) {
        point_interval_ms = (duration_minutes * 60000) / point_count;
        // Ensure minimum interval of 100ms to avoid excessive sampling
        if (point_interval_ms < 100) {
            point_interval_ms = 100;
        }
    } else {
        point_interval_ms = 6000;  // Fallback to 6 seconds
    }
    
    // Load saved duration
    Preferences prefs;
    prefs.begin(namespace_key.c_str(), true);
    duration_minutes = prefs.getInt("duration", default_duration_min);
    prefs.end();
    
    // Recalculate interval if duration was loaded from NVS
    if (point_count > 0) {
        point_interval_ms = (duration_minutes * 60000) / point_count;
        if (point_interval_ms < 100) {
            point_interval_ms = 100;
        }
    }
    
    load_from_nv();
}

void ChartDataHistory::add_point(float value) {
    uint32_t now = millis();
    
    // Check if enough time has passed (using calculated interval based on duration and point count)
    if (now - last_sample_ms < point_interval_ms) {
        return;
    }
    
    last_sample_ms = now;
    
    // Add point in circular buffer
    data[write_index].value = value;
    data[write_index].timestamp_ms = now - start_time_ms;
    
    write_index = (write_index + 1) % CHART_MAX_POINTS;
    if (total_points < CHART_MAX_POINTS) {
        total_points++;
    }
}

void ChartDataHistory::get_points(ChartDataPoint* out_buffer, int& out_count, int max_points) {
    uint32_t now = millis();
    uint32_t window_ms = duration_minutes * 60 * 1000;  // Convert to milliseconds
    uint32_t cutoff_time = (now - start_time_ms) - window_ms;
    
    out_count = 0;
    
    // Iterate backwards from write_index
    for (int i = 0; i < total_points && out_count < max_points; i++) {
        int idx = (write_index - 1 - i + CHART_MAX_POINTS) % CHART_MAX_POINTS;
        
        // Skip old points outside time window
        if (data[idx].timestamp_ms < cutoff_time) {
            break;
        }
        
        out_buffer[out_count] = data[idx];
        out_count++;
    }
    
    // Reverse to chronological order
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
    
    // Load point data
    size_t expected_size = sizeof(ChartDataPoint);
    if (prefs.isKey("data")) {
        size_t read = prefs.getBytes("data", (uint8_t*)data, sizeof(data));
        if (read != sizeof(data)) {
            total_points = 0;  // Corrupted data
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
