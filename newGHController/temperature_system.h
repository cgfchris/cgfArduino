// temperature_system.h
#ifndef TEMPERATURE_SYSTEM_H
#define TEMPERATURE_SYSTEM_H

#include "lvgl.h" // For lv_chart_series_t
#include <stddef.h> // for size_t
#include <time.h>   // for time_t

// Constants from config.h or defined here
// ... (ensure all necessary constants are defined or included from config.h)
#define MAX_TEMP_SAMPLES 60
#define TEMP_SAMPLE_INTERVAL_MS 60000
#define CHART_Y_MIN_VALUE 0
#define CHART_Y_MAX_VALUE 40
#define NTP_TIMEZONE 0
#define MIN_VALID_EPOCH_TIME 1609459200UL


// Structure for historical data
typedef struct {
    float temperature;
    time_t timestamp;
    bool isValidTimestamp;
} TempSampleData;

// Function declarations
void initializeTemperatureSystem();
void updateTemperatureSystem(); // This will now mostly update the chart
void updateCurrentTemperatureFromM4(float m4_temp); 

#endif // TEMPERATURE_SYSTEM_H
