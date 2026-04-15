#ifndef CHART_DATA_HISTORY_H
#define CHART_DATA_HISTORY_H

#include <Arduino.h>
#include <Preferences.h>

#define CHART_MAX_POINTS 300  // Enough for 30min @ 6sec intervals

struct ChartDataPoint {
    float value;
    uint32_t timestamp_ms;  // Milliseconds since start of recording
};

class ChartDataHistory {
private:
    static constexpr const char* kNamespacePrefix = "chart-";
    
    String namespace_key;
    ChartDataPoint data[CHART_MAX_POINTS];
    int write_index;
    int total_points;
    uint32_t start_time_ms;
    uint32_t last_sample_ms;
    int duration_minutes;
    uint32_t point_interval_ms;  // Calculated interval based on duration and point count
    
public:
    // Constructor: key = chart name, default_duration_min = duration in minutes, point_count = number of chart points (100, 150, etc)
    ChartDataHistory(const char* key, int default_duration_min = 10, int point_count = 100);
    
    // Add a data point (will be ignored if too soon)
    void add_point(float value);
    
    // Get points within time window (newest first)
    void get_points(ChartDataPoint* out_buffer, int& out_count, int max_points);
    
    // Clear all data
    void clear();
    
    // Load/save from NVS
    void load_from_nv();
    void save_to_nv();
    
    // Get duration setting
    int get_duration_minutes() const;
    void set_duration_minutes(int minutes);
    
    // Get current point count
    int get_point_count() const { return total_points; }
};

#endif
