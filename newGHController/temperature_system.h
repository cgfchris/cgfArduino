// temperature_system.h
#ifndef TEMPERATURE_SYSTEM_H
#define TEMPERATURE_SYSTEM_H

#include <Arduino.h> // For millis()
#include "lvgl.h"    // For LVGL types (lv_coord_t, lv_chart_series_t)

// Structure to hold sample data (if not already in a more global place, keep it here)
struct TempSampleData {
    float temperature;
    time_t timestamp;
    bool isValidTimestamp;
};

// Initialize the temperature reading system and chart
void initializeTemperatureSystem();

// Update temperature readings, label, and chart
void updateTemperatureSystem();

// Placeholder for actual sensor reading - IMPLEMENT THIS
float readTemperatureSensor();

#endif // TEMPERATURE_SYSTEM_H
