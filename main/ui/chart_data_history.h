#ifndef CHART_DATA_HISTORY_H
#define CHART_DATA_HISTORY_H

#include <Arduino.h>
#include <Preferences.h>

#define CHART_MAX_POINTS 600

struct ChartDataPoint {
    float value;
    uint32_t timestamp_ms;
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
    uint32_t point_interval_ms;
    
public:
    ChartDataHistory(const char* key, int default_duration_min = 10, int point_count = 100);
    
    void add_point(float value);
    
    void get_points(ChartDataPoint* out_buffer, int& out_count, int max_points);
    
    void clear();
    
    void load_from_nv();
    void save_to_nv();
    
    int get_duration_minutes() const;
    void set_duration_minutes(int minutes);
    
    int get_point_count() const { return total_points; }
};

#endif

